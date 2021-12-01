#include "Hosebase/sound.h"

#include "windows.h"
#include "DSound.h"

#pragma comment(lib, "Dsound.lib")

typedef struct {
	
	LPDIRECTSOUND ds;
	LPDIRECTSOUNDBUFFER primary_buffer;
	LPDIRECTSOUNDBUFFER secondary_buffer;

	u32 buffer_size;
	u32 samples_per_second;
	u32 bytes_per_sample;
	u32 write_sample_latency;

	u32 sample_index;
	f32 tsin;

	// TEMP
	Audio audio;

} SoundSystemData;

static SoundSystemData* sound;

///////////////////////////////// LOAD WAV FILE //////////////////////////////////////////

#include "Hosebase/serialize.h"

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

static void fill_sound_buffer(u32 byte_to_lock, u32 bytes_to_write, u32 wave_period, u32 wave_tone)
{
	u32 sample_index = sound->sample_index;
	f32 tsin = sound->tsin;

	void* region0;
	void* region1;
	DWORD region0_size, region1_size;

	if (sound->secondary_buffer->lpVtbl->Lock(sound->secondary_buffer, byte_to_lock, bytes_to_write, &region0, &region0_size, &region1, &region1_size, 0) == DS_OK) {

		u32 sample_count = region0_size / sound->bytes_per_sample;
		i16* samples = region0;
		foreach(index, sample_count) {

			i64 value = (i32)(sinf(tsin) * (f32)wave_tone);

			*samples++ = value;
			*samples++ = value;

			sample_index++;
			tsin += (2.f * PI) / (f32)wave_period;
		}

		sample_count = region1_size / sound->bytes_per_sample;
		samples = region1;
		foreach(index, sample_count) {

			i64 value = (i32)(sinf(tsin) * (f32)wave_tone);

			*samples++ = value;
			*samples++ = value;

			sample_index++;
			tsin += (2.f * PI) / (f32)wave_period;
		}

		sound->secondary_buffer->lpVtbl->Unlock(sound->secondary_buffer, region0, region0_size, region1, region1_size);
	}

	// Reduce the tsin value to mantain his precision
	tsin /= 2.f * PI;
	tsin = tsin - (f32)(i32)tsin;
	tsin *= 2.f * PI;

	sound->sample_index = sample_index;
	sound->tsin = tsin;
}

b8 _sound_initialize(u32 samples_per_second, u32 buffer_size)
{
	sound = memory_allocate(sizeof(SoundSystemData));

	if (!audio_load(&sound->audio, "res/sound/shot.wav")) {
		SV_LOG_ERROR("Can't read the wav file\n");
		return FALSE;
	}

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
		desc.dwBufferBytes = buffer_size;
		desc.lpwfxFormat = &format;

		LPDIRECTSOUNDBUFFER secondary_buffer;

		if (IDirectSound8_CreateSoundBuffer(ds, &desc, &secondary_buffer, 0) != DS_OK) {
			SV_LOG_ERROR("Can't create the sound secondary buffer\n");
			return FALSE;
		}

		sound->secondary_buffer = secondary_buffer;
	}

	sound->buffer_size = buffer_size;
	sound->samples_per_second = samples_per_second;
	sound->bytes_per_sample = sizeof(i16) * 2;
	sound->write_sample_latency = samples_per_second / 20;

	clear_sound_buffer();

	return TRUE;
}

void _sound_update()
{
	// Rate
	/*static f64 last_update = 0.0;
	f64 rate = 1.f / 60.f;

	last_update += core.delta_time;

	if (last_update < rate) {
		return;
	}
	last_update -= rate;*/

	const u32 hz = 250;
	const u32 wave_period = sound->samples_per_second / hz;
	const u32 wave_tone = 20000;

	if (core.frame_count == 0) {
		sound->secondary_buffer->lpVtbl->Play(sound->secondary_buffer, 0, 0, DSBPLAY_LOOPING);
	}

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

			fill_sound_buffer(byte_to_lock, bytes_to_write, wave_period, wave_tone);
		}
	}
}

void _sound_close()
{
	if (sound) {

		memory_free(sound);
	}
}