#if SV_PLATFORM_WINDOWS

#include "Hosebase/sound.h"
#include "Hosebase/platform.h"

#include "windows.h"
#include "DSound.h"

#include "Hosebase/serialize.h"

#pragma comment(lib, "Dsound.lib")

// TODO: Adjust

#define AUDIO_SOURCE_MAX 10000
#define AUDIO_SOURCE_TABLE_SIZE 500

#define AudioSourceFlag_Valid SV_BIT(0)
#define AudioSourceFlag_Stopped SV_BIT(1)

#define AUDIO_INSTANCE_MAX 1000

typedef struct AudioSource AudioSource;

struct AudioSource {
	AudioProperties props;
	Asset audio_asset;
	u32 begin_sample_index;
	u32 flags;
	u64 hash;
	AudioSource* next;
};

typedef struct {
	AudioProperties props;
	Asset audio_asset;
	u32 begin_sample_index;
} AudioInstance;

typedef struct {
	AudioProperties props;
	v3 direction;
} AudioListener;

typedef struct {
	
	LPDIRECTSOUND ds;
	LPDIRECTSOUNDBUFFER primary_buffer;
	LPDIRECTSOUNDBUFFER secondary_buffer;

	u32 buffer_size;
	u32 samples_per_second;
	u32 bytes_per_sample;
	u32 write_sample_latency;

	u32 sample_index;
	f32* samples;

	AudioSource sources[AUDIO_SOURCE_MAX];
	u32 source_count;

	AudioSource* free_sources;

	AudioSource* source_table[AUDIO_SOURCE_TABLE_SIZE];

	AudioInstance instances[AUDIO_INSTANCE_MAX];
	u32 instance_count;
	u32 instance_free_count;
	Mutex mutex_instance;

	AudioListener listener;

	Mutex mutex_listener;

	Thread thread;
	volatile b8 close_request;
	
	Mutex mutex_source;

} SoundSystemData;

static SoundSystemData* sound;

inline AudioProperties validate_props(const AudioProperties* props)
{
	AudioProperties p;
	SV_ZERO(p);

	if (props != NULL) {
		p = *props;
		p.volume = SV_MAX(p.volume, 0.f);
		p.sample_velocity = SV_MAX(p.sample_velocity, 0.f);
		p.range = SV_MAX(p.range, 0.f);
	}
	else {
		p.volume = 1.f;
		p.sample_velocity = 1.f;
		p.range = 10.f;
		p.position = v3_zero();
		p.velocity = v3_zero();
	}

	return p;
}

///////////////////////////////////////////////////////////// AUDIO ASSET ////////////////////////////////////////////////

static b8 asset_audio_load_file(void* asset, const char* filepath)
{
	Audio* audio = asset;

	if (!audio_load(audio, filepath)) {
		return FALSE;
	}

	return TRUE;
}

static void asset_audio_free(void* asset)
{
	Audio* audio = asset;
	audio_destroy(audio);
}

static b8 asset_audio_reload_file(void* asset, const char* filepath)
{
	asset_audio_free(asset);
	return asset_audio_load_file(asset, filepath);
}

///////////////////////////// LISTENER ///////////////////////////////

void listener_update(const AudioProperties* props, v3 direction)
{
	mutex_lock(sound->mutex_listener);

	sound->listener.props = validate_props(props);
	sound->listener.direction = direction;

	mutex_unlock(sound->mutex_listener);
}

///////////////////////////////// AUDIO //////////////////////////////////////////

#pragma pack(push)
#pragma pack(1)

#define RIFF_STR(a, b, c, d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

#define RiffStr_RIFF RIFF_STR('R', 'I', 'F', 'F')
#define RiffStr_WAVE RIFF_STR('W', 'A', 'V', 'E')
#define RiffStr_fmt RIFF_STR('f', 'm', 't', ' ')
#define RiffStr_data RIFF_STR('d', 'a', 't', 'a')
#define RiffStr_fact RIFF_STR('f', 'a', 'c', 't')

typedef struct {
	u32 riff;
	u32 file_size;
	u32 wave;
} WaveHeader;

typedef struct {
	u16 format_tag;
	u16 channel_count;
	u32 samples_per_second;
	u32 avg_bytes_per_second;
	u16 block_align;
	u16 bits_per_sample;
	u16 extension_size;
	u16 valid_bits;
	u32 channel_mask;
	u8 sub_format[16];
} WaveFmt;

#pragma pack(pop)

b8 audio_load(Audio* audio, const char* filepath)
{
	Deserializer s;
	u8* data;
	u32 size;

	b8 res = TRUE;

	if (file_read_binary(FilepathType_Asset, filepath, &data, &size)) {

		deserializer_begin_buffer(&s, data, size);

		WaveHeader header;
		deserializer_read(&s, &header, sizeof(WaveHeader));

		if (header.riff != RiffStr_RIFF || header.wave != RiffStr_WAVE || header.file_size != size - 8) {
			SV_LOG_ERROR("That's an invalid .wav format '%s'\n", filepath);
			goto corrupted;
		}

		WaveFmt* fmt;
		b8 read_fmt = FALSE;
		void* sample_data = NULL;
		u32 sample_bytes = 0;

		while (s.cursor < size) {

			u32 chunk_id;
			u32 chunk_size;

			deserialize_u32(&s, &chunk_id);
			deserialize_u32(&s, &chunk_size);

			u32 begin_offset = s.cursor;

			switch (chunk_id)
			{

			case RiffStr_fmt:
			{
				fmt = (WaveFmt*)(s.data + s.cursor);

				if (fmt->format_tag == 1) {

					if (fmt->channel_count < 1 || fmt->channel_count > 2) {
						SV_LOG_ERROR("Invalid channel count %u in '%s'\n", fmt->channel_count, filepath);
						break;
					}

					read_fmt = TRUE;
				}
				else {
					SV_LOG_ERROR("Only the PCM format is supported\n");
				}

			}
			break;

			case RiffStr_data:
			{
				sample_data = s.data + s.cursor;
				sample_bytes = chunk_size;
			}
			break;

			}


			begin_offset += chunk_size;
			if (chunk_size & 1)
				begin_offset++;
			s.cursor = begin_offset;
		}

		if (!read_fmt) {
			SV_LOG_ERROR("A valid fmt block is not found in '%s'\n", filepath);
			goto corrupted;
		}
		if (sample_bytes == 0) {
			SV_LOG_ERROR("There are no samples in '%s'\n", filepath);
			goto corrupted;
		}

		{
			audio->channel_count = fmt->channel_count;

			u32 sample_count = (sample_bytes / (fmt->bits_per_sample / 8)) / fmt->channel_count; // Per channel sample count
			u32 per_channel_bytes = sizeof(i16) * sample_count;

			audio->sample_count = sample_count;
			audio->samples_per_second = fmt->samples_per_second;

			// TODO: Support 8 bits samples

			if (audio->channel_count == 1) {
				audio->samples[0] = memory_allocate(per_channel_bytes);
				audio->samples[1] = NULL;

				deserializer_read(&s, audio->samples[0], per_channel_bytes);
			}
			else if (audio->channel_count == 2) {
				u8* mem = memory_allocate(per_channel_bytes * 2);
				audio->samples[0] = (i16*)mem;
				mem += per_channel_bytes;
				audio->samples[1] = (i16*)mem;

				i16* src = (i16*)sample_data;
				foreach(i, sample_count) {

					audio->samples[0][i] = *src++;
					audio->samples[1][i] = *src++;
				}
			}
		}

		goto free_data;
	corrupted:
		res = FALSE;

	free_data:
		deserializer_end_file(&s);
		memory_free(data);
	}
	else res = FALSE;

	return res;
}

void audio_destroy(Audio* audio)
{
	if (audio == NULL)
		return;

	if (audio->samples[0])
		memory_free(audio->samples[0]);
}

inline void free_audio_instance(AudioInstance* inst)
{
	asset_decrement(inst->audio_asset);
	memory_zero(inst, sizeof(AudioInstance));
	sound->instance_free_count++;
	// TODO: Decrease instance_free_count
}

void audio_play(Asset audio_asset, const AudioProperties* props)
{
	AudioInstance* inst = NULL;

	mutex_lock(sound->mutex_instance);

	if (sound->instance_free_count) {

		foreach(i, sound->instance_count) {

			AudioInstance* instance = sound->instances + i;
			if (instance->audio_asset == 0) {
				inst = instance;
				break;
			}
		}

		if (inst != NULL)
			sound->instance_free_count--;

		assert(inst);
	}

	if (inst == NULL) {

		if (sound->instance_count >= AUDIO_INSTANCE_MAX) {
			SV_LOG_WARNING("Audio instances exceeded, the limit is %u\n", AUDIO_INSTANCE_MAX);
		}
		else inst = sound->instances + sound->instance_count++;
	}

	mutex_unlock(sound->mutex_instance);

	if (inst != NULL) {

		inst->props = validate_props(props);
		inst->begin_sample_index = sound->sample_index;
		inst->audio_asset = audio_asset;

		asset_increment(inst->audio_asset);
	}
}

inline void free_audio_source(u64 hash);

void audio_stop()
{
	// Audio instances
	{
		mutex_lock(sound->mutex_instance);

		foreach(i, sound->instance_count) {

			AudioInstance* inst = sound->instances + i;
			if (inst->audio_asset != 0) {
				free_audio_instance(inst);
			}
		}

		sound->instance_count = 0;
		sound->instance_free_count = 0;

		mutex_unlock(sound->mutex_instance);
	}

	// Audio sources
	{
		audio_source_lock();

		foreach(source_index, sound->source_count) {

			AudioSource* src = sound->sources + source_index;
			free_audio_source(src->hash);
		}

		audio_source_unlock();
	}
}

///////////////////////////////// AUDIO SOURCE //////////////////////////////////////////

// Add a valid asset to create a new one if doesn't exists
inline AudioSource* get_audio_source(u64 hash, Asset audio_asset)
{
	AudioSource* src = NULL;
	AudioSource* parent = NULL;

	AudioSource** source = sound->source_table + hash % AUDIO_SOURCE_TABLE_SIZE;

	if (*source) {

		src = *source;

		while (src->hash != hash) {

			parent = src;
			src = src->next;
			if (src == NULL) {
				break;
			}
		}
	}

	if (src == NULL && audio_asset) {

		// Create a new one
		{
			if (sound->free_sources) {
				src = sound->free_sources;
				sound->free_sources = src->next;
			}

			if (src == NULL) {
				if (sound->source_count >= AUDIO_SOURCE_MAX) {
					SV_LOG_WARNING("Audio source playing limit is '%u'\n", AUDIO_SOURCE_MAX);
					return NULL;
				}

				src = sound->sources + sound->source_count++;
			}

			src->flags = AudioSourceFlag_Valid;
			src->hash = hash;
			src->next = NULL;
			src->audio_asset = audio_asset;

			asset_increment(src->audio_asset);
		}

		if (parent != NULL) {

			parent->next = src;
		}
		else *source = src;
	}

	return src;
}

inline void free_audio_source(u64 hash)
{
	AudioSource* src = NULL;
	AudioSource* parent = NULL;

	AudioSource** source = sound->source_table + hash % AUDIO_SOURCE_TABLE_SIZE;

	if (*source) {

		src = *source;

		while (src->hash != hash) {

			parent = src;
			src = src->next;
			if (src == NULL) {
				break;
			}
		}

		if (src != NULL) {

			if (parent != NULL) {
				parent->next = src->next;
			}
			else {
				*source = src->next;
			}

			// TODO: Erase from free link list as much as possible

			asset_decrement(src->audio_asset);

			memory_zero(src, sizeof(AudioSource));

			// Add to link list
			src->next = sound->free_sources;
			sound->free_sources = src;
		}
	}
}

void audio_source_lock()
{
	mutex_lock(sound->mutex_source);
}

void audio_source_unlock()
{
	mutex_unlock(sound->mutex_source);
}

void audio_source_desc(u64 id, const AudioProperties* props)
{
	u64 hash = id;
	
	AudioSource* src = get_audio_source(hash, 0);

	if (src == NULL)
		return;

	src->props = validate_props(props);
}

void audio_source_continue(u64 id)
{
	u64 hash = id;
	AudioSource* src = get_audio_source(hash, 0);

	if (src == NULL)
		return;

	src->flags &= ~AudioSourceFlag_Stopped;
}

void audio_source_pause(u64 id)
{
	u64 hash = id;
	AudioSource* src = get_audio_source(hash, 0);

	if (src == NULL)
		return;

	src->flags |= AudioSourceFlag_Stopped;
}

void audio_source_kill(u64 id)
{
	free_audio_source(id);
}

b8 audio_source_is_playing(u64 id)
{
	u64 hash = id;
	AudioSource* src = get_audio_source(hash, 0);

	if (src == NULL)
		return FALSE;
	
	return src->audio_asset != 0 && !(src->flags & AudioSourceFlag_Stopped);
}

void audio_source_play(u64 id, Asset audio_asset, const AudioProperties* source_props, const AudioProperties* audio_props)
{
	u64 hash = id;
	AudioSource* src = get_audio_source(hash, audio_asset);

	// TODO: Play individual sounds

	if (src == NULL)
		return;

	src->props = validate_props(source_props);
	src->begin_sample_index = sound->sample_index;
}

/////////////////////////////////////////////////// MUSIC ////////////////////////////////////////////////////////

void music_play(u64 id, Asset audio_asset, const AudioProperties* props)
{
	// TODO
}

void music_desc(u64 id, const AudioProperties* props)
{
	// TODO
}

void music_continue(u64 id)
{
	// TODO
}

void music_pause(u64 id)
{
	// TODO
}

void music_stop(u64 id)
{
	// TODO
}

///////////////////////////////////////////////////////// SOUND BUFFER UTILS ////////////////////////////////////////////

static void clear_sound_buffer()
{
	void* region0;
	void* region1;
	DWORD region0_size, region1_size;

	if (sound->secondary_buffer->lpVtbl->Lock(sound->secondary_buffer, 0, sound->buffer_size, &region0, &region0_size, &region1, &region1_size, 0) == DS_OK) {

		memory_zero(region0, region0_size);
		memory_zero(region1, region1_size);

		sound->secondary_buffer->lpVtbl->Unlock(sound->secondary_buffer, region0, region0_size, region1, region1_size);
	}
}

static void fill_sound_buffer(u32 byte_to_lock, u32 bytes_to_write)
{
	void* regions[2];
	DWORD region_sizes[2];

	if (sound->secondary_buffer->lpVtbl->Lock(sound->secondary_buffer, byte_to_lock, bytes_to_write, &regions[0], &region_sizes[0], &regions[1], &region_sizes[1], 0) == DS_OK) {

		f32* src = sound->samples;

		foreach(buffer_index, 2) {

			u32 sample_count = region_sizes[buffer_index] / sound->bytes_per_sample;
			i16* samples = regions[buffer_index];
			foreach(index, sample_count) {

				f32 left = *src++;
				f32 right = *src++;

				left = math_clamp(-32768.f, left, 32767.f);
				right = math_clamp(-32768.f, right, 32767.f);

				*samples++ = (i16)left;
				*samples++ = (i16)right;
			}
		}

		sound->secondary_buffer->lpVtbl->Unlock(sound->secondary_buffer, regions[0], region_sizes[0], regions[1], region_sizes[1]);
	}
}

static b8 write_audio(u32* begin_sample_index, Asset audio_asset, const AudioProperties* props, const AudioListener* listener, u32 samples_to_write)
{
	const Audio* audio = asset_get(audio_asset);

	// TODO: 
	// - Variable velocity
	// - Use two loops, one for each channel

	if (audio == NULL)
		return TRUE;

	f32 v0 = props->volume;
	f32 v1 = props->volume;

	// 3D effect
	if ((listener->props.flags & AudioFlag_3D) && (props->flags & AudioFlag_3D)){

		v3 to = v3_sub(props->position, listener->props.position);
		f32 to_distance = v3_length(to);

		if (to_distance > 0.00001f) {

			v3 to_normalized = v3_div_scalar(to, to_distance);

			// Fade off
			{
				const f32 range = SV_MIN(props->range, listener->props.range);

				const f32 min_dist = 1.f;
				const f32 dist_range = SV_MAX(range - min_dist, 0.f);

				if (dist_range > 0.00001f) {

					f32 dist = SV_MAX(to_distance, min_dist) - min_dist;

					f32 mul = dist / dist_range;
					mul = 1.f - math_smooth(mul, 4.f);

					v0 *= mul;
					v1 *= mul;
				}
			}

			// Stereo
			{
				v3 dir = listener->direction;

				v2 move_dir = v2_normalize(v2_set(dir.x, dir.z));

				v2 move_ear_dir = v2_perpendicular(move_dir);

				v3 l_ear_dir = dir;
				l_ear_dir.x += -move_ear_dir.x * 1.8f;
				l_ear_dir.z += -move_ear_dir.y * 1.8f;
				l_ear_dir = v3_normalize(l_ear_dir);

				v3 r_ear_dir = dir;
				r_ear_dir.x += move_ear_dir.x * 1.8f;
				r_ear_dir.z += move_ear_dir.y * 1.8f;
				r_ear_dir = v3_normalize(r_ear_dir);

				const f32 factor = 0.85f;

				f32 mul0 = (1.f - factor) + ((v3_dot(l_ear_dir, to_normalized) * 0.5f + 0.5f) * 0.85f + 0.15f) * factor;
				f32 mul1 = (1.f - factor) + ((v3_dot(r_ear_dir, to_normalized) * 0.5f + 0.5f) * 0.85f + 0.15f) * factor;

				v0 *= mul0;
				v1 *= mul1;
			}

			// TODO: Doppler effect (but first need a way of modify the pitch)
		}
	}

	if (v0 < 0.0001f && v1 < 0.0001f)
		return TRUE;

	u32 end_sample_index;
	{
		f32 seconds = (f32)audio->sample_count / (f32)audio->samples_per_second;
		seconds *= props->sample_velocity;

		end_sample_index = *begin_sample_index + (u32)(seconds * (f32)sound->samples_per_second);
	}

	if (end_sample_index > sound->sample_index) {

		u32 sample_index = sound->sample_index - *begin_sample_index;

		u32 write_count = SV_MIN(samples_to_write, (end_sample_index - sound->sample_index));

		foreach(i, write_count) {

			u32 s0 = (sample_index + SV_MAX(i, 1u) - 1) % audio->sample_count;
			u32 s1 = (sample_index + i) % audio->sample_count;

			s0 = (u32)(((f32)s0 / (f32)sound->samples_per_second) * (f32)audio->samples_per_second * props->sample_velocity);
			s1 = (u32)(((f32)s1 / (f32)sound->samples_per_second) * (f32)audio->samples_per_second * props->sample_velocity);

			s0 %= audio->sample_count;
			s1 %= audio->sample_count;

			if (s0 > s1) {
				u32 aux = s0;
				s0 = s1;
				s1 = aux;
			}

			f32 left_value = 0;
			f32 right_value = 0;

			// I do it multiplying individualy the samples to have more precision
			f32 mult = 1.f / (f32)(s1 - s0 + 1);

			for (u32 s = s0; s <= s1; ++s) {

				left_value += (f32)audio->samples[0][s] * mult;
				right_value += (f32)audio->samples[1][s] * mult;
			}

			sound->samples[i * 2 + 0] += left_value * v0;
			sound->samples[i * 2 + 1] += right_value * v1;
		}

		return TRUE;
	}
	
	return FALSE;
}

static i32 thread_main(void* _data)
{
	const u32 updates_per_second = 500;

	while (!sound->close_request) {

		f64 start_time = timer_now();

		{
			DWORD play_cursor, write_cursor;

			if (sound->secondary_buffer->lpVtbl->GetCurrentPosition(sound->secondary_buffer, &play_cursor, &write_cursor) == DS_OK) {

				DWORD byte_to_lock = (sound->sample_index * sound->bytes_per_sample) % sound->buffer_size;
				DWORD bytes_to_write;

				DWORD target_cursor = (play_cursor + (sound->write_sample_latency * sound->bytes_per_sample)) % sound->buffer_size;

				if (byte_to_lock == target_cursor) {
					bytes_to_write = 0;
				}
				else if (byte_to_lock > target_cursor) {
					bytes_to_write = sound->buffer_size - byte_to_lock + target_cursor;
				}
				else {
					bytes_to_write = target_cursor - byte_to_lock;
				}

				//TODO: Should limit the bytes_to_write value?

				if (bytes_to_write) {

					u32 samples_to_write = bytes_to_write / sound->bytes_per_sample;

					AudioListener listener;
					{
						mutex_lock(sound->mutex_listener);
						listener = sound->listener;
						mutex_unlock(sound->mutex_listener);
					}

					// Clear buffer
					{
						foreach(i, samples_to_write) {
							sound->samples[i * 2 + 0] = 0.f;
							sound->samples[i * 2 + 1] = 0.f;
						}
					}

					// Append audio instances
					{
						mutex_lock(sound->mutex_instance);

						foreach(instance_index, sound->instance_count) {

							AudioInstance* inst = sound->instances + instance_index;

							if (!write_audio(&inst->begin_sample_index, inst->audio_asset, &inst->props, &listener, samples_to_write)) {

								free_audio_instance(inst);
							}
						}

						mutex_unlock(sound->mutex_instance);
					}

					// Append audio sources
					{
						audio_source_lock();

						foreach(source_index, sound->source_count) {

							AudioSource* src = sound->sources + source_index;

							if (!write_audio(&src->begin_sample_index, src->audio_asset, &src->props, &listener, samples_to_write)) {

								free_audio_source(src->hash);
							}
						}

						audio_source_unlock();
					}

					fill_sound_buffer(byte_to_lock, bytes_to_write);

					sound->sample_index += samples_to_write;
				}
			}
		}

		f64 ellapsed = timer_now() - start_time;
		f64 wait = (1.0 / (f64)updates_per_second) - ellapsed;

		thread_sleep((f64)(SV_MAX(wait, 0.0) * 1000.0));
	}

	return 0;
}

b8 sound_initialize(u32 samples_per_second)
{
	sound = memory_allocate(sizeof(SoundSystemData));

	sound->bytes_per_sample = sizeof(i16) * 2;
	sound->buffer_size = samples_per_second * sound->bytes_per_sample;
	sound->samples_per_second = samples_per_second;
	sound->write_sample_latency = samples_per_second / 15;

	if (DirectSoundCreate(NULL, &sound->ds, NULL) != DS_OK) {
		return FALSE;
	}

	LPDIRECTSOUND ds = sound->ds;

	if (IDirectSound8_SetCooperativeLevel(ds, (HWND)window_handle(), DSSCL_PRIORITY) != DS_OK) {
		SV_LOG_ERROR("Can't set the priority cooperative level in sound system\n");
	}

	WAVEFORMATEX format;
	SV_ZERO(format);
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = samples_per_second;
	format.wBitsPerSample = 16;
	format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize = 0;

	// Create primary buffer
	{
		DSBUFFERDESC desc;
		SV_ZERO(desc);

		desc.dwSize = sizeof(DSBUFFERDESC);
		desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

		LPDIRECTSOUNDBUFFER primary_buffer;

		if (IDirectSound8_CreateSoundBuffer(ds, &desc, &primary_buffer, 0) != DS_OK) {
			SV_LOG_ERROR("Can't create the sound primary buffer\n");
			return FALSE;
		}
		if (primary_buffer->lpVtbl->SetFormat(primary_buffer, &format) != DS_OK) {
			SV_LOG_ERROR("Can't set the sound primary buffer format\n");
			return FALSE;
		}

		sound->primary_buffer = primary_buffer;
	}

	// Create secondary buffer
	{
		DSBUFFERDESC desc;
		SV_ZERO(desc);

		desc.dwSize = sizeof(DSBUFFERDESC);
		desc.dwFlags = 0;
		desc.dwBufferBytes = sound->buffer_size;
		desc.lpwfxFormat = &format;

		LPDIRECTSOUNDBUFFER secondary_buffer;

		if (IDirectSound8_CreateSoundBuffer(ds, &desc, &secondary_buffer, 0) != DS_OK) {
			SV_LOG_ERROR("Can't create the sound secondary buffer\n");
			return FALSE;
		}

		sound->secondary_buffer = secondary_buffer;
	}

	clear_sound_buffer();
	sound->secondary_buffer->lpVtbl->Play(sound->secondary_buffer, 0, 0, DSBPLAY_LOOPING);

	// Register audio asset
	{
		AssetTypeDesc desc;
		const char* extensions[10];
		desc.extensions = extensions;

		desc.name = "audio";
		desc.asset_size = sizeof(Audio);
		desc.extensions[0] = "wav";
		desc.extensions[1] = "WAV";
		desc.extension_count = 2;
		desc.load_file_fn = asset_audio_load_file;
		desc.reload_file_fn = asset_audio_reload_file;
		desc.free_fn = asset_audio_free;
		desc.unused_time = 5.f;

		SV_CHECK(asset_register_type(&desc));
	}

	// Allocate samples data
	sound->samples = memory_allocate(sound->samples_per_second * 2.f * sizeof(f32));

	// Mutex
	sound->mutex_source = mutex_create();
	sound->mutex_instance = mutex_create();
	sound->mutex_listener = mutex_create();

	// Run thread
	sound->thread = thread_create(thread_main, NULL);

	if (sound->thread == 0) {
		SV_LOG_ERROR("Can't create the sound thread\n");
		return FALSE;
	}

	// Configure thread
	{
		HANDLE th = (HANDLE)sound->thread;

		// Put the thread in a dedicated hardware core
		{
			DWORD_PTR mask = u64_max;
			DWORD_PTR res = SetThreadAffinityMask(th, mask);
			assert(res > 0);
		}

		// Set thread priority
		{
			BOOL res = SetThreadPriority(th, THREAD_PRIORITY_HIGHEST);
			assert(res != 0);
		}

#if SV_SLOW
		// Set thread name
		{
			HRESULT hr = SetThreadDescription(th, (PCWSTR)("sound"));
			assert(SUCCEEDED(hr));
		}
#endif
	}

	return TRUE;
}

void sound_close()
{
	if (sound) {
		// TODO
		sound->close_request = TRUE;

		thread_wait(sound->thread);

		mutex_destroy(sound->mutex_source);
		mutex_destroy(sound->mutex_instance);
		mutex_destroy(sound->mutex_listener);

		memory_free(sound->samples);
		memory_free(sound);
	}
}
#endif