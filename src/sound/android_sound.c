#if SV_PLATFORM_ANDROID

#include "sound_internal.h"

#include <aaudio/AAudio.h>

typedef struct
{
    u32 samples_per_second;

    AAudioStream *stream;
    b8 recreate_stream_request;

    i32* write_buffer;
    u32 write_buffer_size;
} SoundAndroidData;

static SoundAndroidData *sound;

static b8 create_stream()
{
    if (sound->stream != NULL)
    {
        AAudioStream_close(sound->stream);
        sound->stream = NULL;
    }

    AAudioStreamBuilder *builder;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);

    if (result < 0)
    {
        SV_LOG_ERROR("Can't create AudioStreamBuilder: %i\n", result);
        return FALSE;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setSampleRate(builder, sound->samples_per_second);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    // AAudioStreamBuilder_setBufferCapacityInFrames(builder, frames);

    result = AAudioStreamBuilder_openStream(builder, &sound->stream);
    AAudioStreamBuilder_delete(builder);

    if (result < 0)
    {
        sound->stream = NULL;
        SV_LOG_ERROR("Can't create AudioStream: %i\n", result);
        return FALSE;
    }

    AAudioStream_requestStart(sound->stream);

    return TRUE;
}

b8 sound_platform_initialize(u32 samples_per_second)
{
    sound = memory_allocate(sizeof(SoundAndroidData));
    sound->samples_per_second = samples_per_second;

    create_stream();

    return TRUE;
}

void sound_platform_close()
{
    if (sound != NULL)
    {
        if (sound->write_buffer != NULL)
            memory_free(sound->write_buffer);

        if (sound->stream != NULL)
            AAudioStream_close(sound->stream);

        memory_free(sound);
    }
}

void sound_configure_thread(Thread thread)
{
    // TODO:
}

void sound_compute_sample_range(u32 sample_index, u32 *sample_count, u32 *offset)
{
    if (sound->recreate_stream_request)
    {
        sound->recreate_stream_request = FALSE;
        create_stream();
    }

    *offset = 0;

    if (sound->stream == NULL)
    {
        *sample_count = 0;
    }
    else
    {
        *sample_count = AAudioStream_getFramesPerBurst(sound->stream);
    }
}

void sound_fill_buffer(f32 *samples, u32 sample_count, u32 offset)
{
    if (sound->stream == NULL)
    {
        sound->recreate_stream_request = TRUE;
        thread_sleep(1000);
        return;
    }

    u32 channel_count = AAudioStream_getChannelCount(sound->stream);

    if (sound->write_buffer_size < (sample_count * channel_count))
    {
        if (sound->write_buffer != NULL)
        {
            memory_free(sound->write_buffer);
            sound->write_buffer = NULL;
        }

        sound->write_buffer_size = sample_count * channel_count * 2;
        sound->write_buffer = memory_allocate(sound->write_buffer_size * sizeof(i16));
    }

    i32* write_buffer;
    u32 write_buffer_size;

    // Parse data to i16
    {
        f32* src = samples;
        i16* dst = sound->write_buffer;

        foreach (i, sample_count)
        {
            f32 left = *src++;
            f32 right = *src++;

            left = math_clamp(-32768.f, left, 32767.f);
            right = math_clamp(-32768.f, right, 32767.f);

            *dst++ = (i16)left;
            *dst++ = (i16)right;

            assert(dst <= sound->write_buffer + sound->write_buffer_size);
        }
    }

    aaudio_result_t result = AAudioStream_write(sound->stream, sound->write_buffer, sample_count, 1000 * 1000000);

    if (result < 0)
    {
        // Device is changed
        if (result == -899)
        {
            sound->recreate_stream_request = TRUE;
        }
        // Undefined error
        else
            SV_LOG_ERROR("Audio Write: %i\n", result);
    }
}

#endif