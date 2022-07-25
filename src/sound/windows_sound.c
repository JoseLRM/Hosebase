#if SV_PLATFORM_WINDOWS

#include "sound_internal.h"

#include "windows.h"
#include "DSound.h"
#pragma comment(lib, "Dsound.lib")

typedef struct
{

	u32 buffer_size;
	u32 bytes_per_sample;
	u32 write_sample_latency;

	LPDIRECTSOUND ds;
	LPDIRECTSOUNDBUFFER primary_buffer;
	LPDIRECTSOUNDBUFFER secondary_buffer;
} SoundWindowsData;

static SoundWindowsData *sound;

static void clear_sound_buffer()
{
	void *region0;
	void *region1;
	DWORD region0_size, region1_size;

	if (sound->secondary_buffer->lpVtbl->Lock(sound->secondary_buffer, 0, sound->buffer_size, &region0, &region0_size, &region1, &region1_size, 0) == DS_OK)
	{
		memory_zero(region0, region0_size);
		memory_zero(region1, region1_size);

		sound->secondary_buffer->lpVtbl->Unlock(sound->secondary_buffer, region0, region0_size, region1, region1_size);
	}
}

b8 sound_platform_initialize(u32 samples_per_second)
{
	sound = memory_allocate(sizeof(SoundWindowsData));

	sound->bytes_per_sample = sizeof(i16) * 2;
	sound->buffer_size = samples_per_second * sound->bytes_per_sample;
	sound->write_sample_latency = samples_per_second / 15;

	if (DirectSoundCreate(NULL, &sound->ds, NULL) != DS_OK)
	{
		return FALSE;
	}

	LPDIRECTSOUND ds = sound->ds;

	if (IDirectSound8_SetCooperativeLevel(ds, (HWND)window_handle(), DSSCL_PRIORITY) != DS_OK)
	{
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

		if (IDirectSound8_CreateSoundBuffer(ds, &desc, &primary_buffer, 0) != DS_OK)
		{
			SV_LOG_ERROR("Can't create the sound primary buffer\n");
			return FALSE;
		}
		if (primary_buffer->lpVtbl->SetFormat(primary_buffer, &format) != DS_OK)
		{
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

		if (IDirectSound8_CreateSoundBuffer(ds, &desc, &secondary_buffer, 0) != DS_OK)
		{
			SV_LOG_ERROR("Can't create the sound secondary buffer\n");
			return FALSE;
		}

		sound->secondary_buffer = secondary_buffer;
	}

	clear_sound_buffer();
	sound->secondary_buffer->lpVtbl->Play(sound->secondary_buffer, 0, 0, DSBPLAY_LOOPING);

	return TRUE;
}

void sound_configure_thread(Thread thread)
{
	HANDLE th = (HANDLE)thread;

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

void sound_platform_close()
{
	// TODO:
}

void sound_compute_sample_range(u32 sample_index, u32* sample_count, u32* offset)
{
	*sample_count = 0;
	*offset = 0;

	DWORD play_cursor, write_cursor;

	if (sound->secondary_buffer->lpVtbl->GetCurrentPosition(sound->secondary_buffer, &play_cursor, &write_cursor) == DS_OK)
	{
		DWORD byte_to_lock = (sample_index * sound->bytes_per_sample) % sound->buffer_size;
		DWORD bytes_to_write;

		DWORD target_cursor = (play_cursor + (sound->write_sample_latency * sound->bytes_per_sample)) % sound->buffer_size;

		if (byte_to_lock == target_cursor)
		{
			bytes_to_write = 0;
		}
		else if (byte_to_lock > target_cursor)
		{
			bytes_to_write = sound->buffer_size - byte_to_lock + target_cursor;
		}
		else
		{
			bytes_to_write = target_cursor - byte_to_lock;
		}

		*sample_count = bytes_to_write / sound->bytes_per_sample;
		*offset = byte_to_lock;
	}
}

void sound_fill_buffer(f32 *samples, u32 sample_count, u32 offset)
{
	void *regions[2];
	DWORD region_sizes[2];

	DWORD byte_to_lock = offset;
	DWORD bytes_to_write = sample_count * sound->bytes_per_sample;

	if (sound->secondary_buffer->lpVtbl->Lock(sound->secondary_buffer, byte_to_lock, bytes_to_write, &regions[0], &region_sizes[0], &regions[1], &region_sizes[1], 0) == DS_OK)
	{
		f32 *src = samples;

		foreach (buffer_index, 2)
		{

			u32 sample_count = region_sizes[buffer_index] / sound->bytes_per_sample;
			i16 *samples = regions[buffer_index];
			foreach (index, sample_count)
			{

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

#endif