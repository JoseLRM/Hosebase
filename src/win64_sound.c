#include "Hosebase/sound.h"

#include "windows.h"
#include "DSound.h"

#include "Hosebase/serialize.h"

#pragma comment(lib, "Dsound.lib")

typedef struct {
	u32 begin_sample_index;
	AudioDesc desc;
} AudioInstance;

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

	AudioInstance instance;

} SoundSystemData;

static SoundSystemData* sound;

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

	// sound->sample_index = sample_index;
	// sound->tsin = tsin;
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

b8 _sound_initialize(u32 samples_per_second)
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

	// Allocate samples data
	sound->samples = memory_allocate(sound->samples_per_second * 2.f * sizeof(f32));

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
		desc.unused_time = 10.f;

		SV_CHECK(asset_register_type(&desc));
	}

	return TRUE;
}

// TEMP
#include "input.h"

void _sound_update()
{
	// TEMP
	if (input_key(Key_S, InputState_Pressed)) {

		AudioDesc desc;
		SV_ZERO(desc);
		desc.audio_asset = asset_load_from_file("C:/Users/isca/Downloads/yt1s.com - Se Menea.wav", AssetPriority_RightNow);
		desc.volume = 0.5f;
		desc.velocity = 1.f;

		audio_play_desc(&desc);
	}

	// TODO: If the audio instance count is 0 and the sampler_index is high: Decrese sampler_index to avoid precision issues

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

				// Clear buffer
				{
					foreach(i, samples_to_write) {
						sound->samples[i * 2 + 0] = 0.f;
						sound->samples[i * 2 + 1] = 0.f;
					}
				}

				// Append audio instances
				{
					AudioInstance* inst = &sound->instance;
					AudioDesc* desc = &inst->desc;

					Audio* audio = asset_get(desc->audio_asset);

					// TODO: 
					// - Variable velocity
					// - Decrement the asset when the audio instance is finished
					// - Use two loops, one for each channel

					u32 end_sample_index = 0;
					if (audio) {
						f32 seconds = (f32)audio->sample_count / (f32)audio->samples_per_second;
						seconds *= desc->velocity;

						end_sample_index = sound->sample_index + (u32)(seconds * (f32)sound->samples_per_second);
					}

					if (audio != NULL && end_sample_index > sound->sample_index) {

						u32 sample_index = sound->sample_index - inst->begin_sample_index;

						u32 write_count = SV_MIN(samples_to_write, (end_sample_index - sound->sample_index));

						foreach(i, write_count) {

							u32 s0 = (sample_index + SV_MAX(i, 1u) - 1) % audio->sample_count;
							u32 s1 = (sample_index + i) % audio->sample_count;

							s0 = (u32)(((f32)s0 / (f32)sound->samples_per_second) * (f32)audio->samples_per_second * desc->velocity);
							s1 = (u32)(((f32)s1 / (f32)sound->samples_per_second) * (f32)audio->samples_per_second * desc->velocity);

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

							sound->samples[i * 2 + 0] += left_value * desc->volume;
							sound->samples[i * 2 + 1] += right_value * desc->volume;
						}
					}
				}

				fill_sound_buffer(byte_to_lock, bytes_to_write);

				sound->sample_index += samples_to_write;
			}
		}
	}
}

void _sound_close()
{
	if (sound) {

		memory_free(sound->samples);
		memory_free(sound);
	}
}

///////////////////////////////// AUDIO OBJECT //////////////////////////////////////////

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

	if (file_read_binary(filepath, &data, &size)) {

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

void audio_play_desc(const AudioDesc* desc)
{
	AudioInstance* inst = &sound->instance;
	inst->desc = *desc;

	asset_increment(inst->desc.audio_asset);

	inst->begin_sample_index = sound->sample_index;
}