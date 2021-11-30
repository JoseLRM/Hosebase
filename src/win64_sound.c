#include "Hosebase/sound.h"

#include "windows.h"
#include "DSound.h"

#pragma comment(lib, "Dsound.lib")

typedef struct {
	
	LPDIRECTSOUND ds;
	LPDIRECTSOUNDBUFFER primary_buffer;
	LPDIRECTSOUNDBUFFER secondary_buffer;

	u32 buffer_size;

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
		sound->buffer_size = buffer_size;
	}

	// TEMP
	sound->secondary_buffer->lpVtbl->Play(sound->secondary_buffer, 0, 0, DSBPLAY_LOOPING);

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

	static u32 sample_index = 0;

	const i32 hz = 200 + sin(timer_now() * 3.f) * 100.f;
	const i32 square_wave_period = 48800 / hz;

	DWORD play_cursor, write_cursor;

	if (sound->secondary_buffer->lpVtbl->GetCurrentPosition(sound->secondary_buffer, &play_cursor, &write_cursor) == DS_OK) {

		DWORD byte_to_lock = sample_index * 4 % sound->buffer_size;
		DWORD bytes_to_write;

		if (byte_to_lock > play_cursor) {
			bytes_to_write = sound->buffer_size - byte_to_lock + play_cursor;
		}
		else {
			bytes_to_write = play_cursor - byte_to_lock;
		}

		void* region0;
		void* region1;
		DWORD region0_size, region1_size;

		if (sound->secondary_buffer->lpVtbl->Lock(sound->secondary_buffer, byte_to_lock, bytes_to_write, &region0, &region0_size, &region1, &region1_size, 0) == DS_OK) {

			u32 sample_count = region0_size / 4;
			i16* samples = region0;
			foreach(index, sample_count) {

				i64 value = (sample_index++ / (square_wave_period / 2) % 2) ? 60000 : -60000;

				*samples++ = value;
				*samples++ = 0;
			}

			sample_count = region1_size / 4;
			samples = region1;
			foreach(index, sample_count) {
				
				i64 value = (sample_index++ / (square_wave_period / 2) % 2) ? 60000 : -60000;

				*samples++ = value;
				*samples++ = 0;
			}

			sound->secondary_buffer->lpVtbl->Unlock(region0, region0_size, region1, region1_size);
		}
	}
}

void _sound_close()
{
	if (sound) {

		memory_free(sound);
	}
}