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

} SoundSystemData;

static SoundSystemData* sound;

b8 _sound_initialize(u32 samples_per_second, u32 buffer_size)
{
	sound = memory_allocate(sizeof(SoundSystemData));

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

	return TRUE;
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

// TEMP
#include "Hosebase/input.h"

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

	const u32 hz = 250 + (u32)((input_mouse_position().x + 0.5f) * 200.f);
	const u32 wave_period = sound->samples_per_second / hz;
	const u32 wave_tone = 20000;

	if (core.frame_count == 0) {
		fill_sound_buffer(0, sound->write_sample_latency * sound->bytes_per_sample, wave_period, wave_tone);
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