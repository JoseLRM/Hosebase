#include "sound_internal.h"
#include "Hosebase/serialize.h"

// TODO: Adjust

#define AUDIO_SOURCE_MAX 10000
#define AUDIO_SOURCE_TABLE_SIZE 500

#define AudioSourceFlag_Valid SV_BIT(0)
#define AudioSourceFlag_Stopped SV_BIT(1)

#define AUDIO_INSTANCE_MAX 1000

#define MUSIC_MAX 100

typedef struct AudioSource AudioSource;

struct AudioSource
{
    AudioProperties props;
    Asset audio_asset;
    u32 begin_sample_index;
    u32 flags;
    u64 hash;
    AudioSource *next;
};

typedef struct
{
    AudioProperties props;
    Asset audio_asset;
    u32 begin_sample_index;
} AudioInstance;

typedef struct
{
    AudioProperties props;
    Asset audio_asset;
    u32 begin_sample_index;
    u64 id;
    u32 fade_out_start;
    f32 fade_out_time;
} Music;

typedef struct
{
    AudioProperties props;
    v3 direction;
} AudioListener;

typedef struct
{
    u32 samples_per_second;

    u32 sample_index;
    f32 *samples;

    AudioSource sources[AUDIO_SOURCE_MAX];
    u32 source_count;

    AudioSource *free_sources;

    AudioSource *source_table[AUDIO_SOURCE_TABLE_SIZE];
    Mutex mutex_source;

    AudioInstance instances[AUDIO_INSTANCE_MAX];
    u32 instance_count;
    u32 instance_free_count;
    Mutex mutex_instance;

    AudioListener listener;
    Mutex mutex_listener;

    Music music[MUSIC_MAX];
    u32 music_count;
    u32 music_free_count;
    Mutex mutex_music;

    Thread thread;
    volatile b8 close_request;

} SoundSystemData;

static SoundSystemData *sound;

inline AudioProperties validate_props(const AudioProperties *props)
{
    AudioProperties p;
    SV_ZERO(p);

    if (props != NULL)
    {
        p = *props;
        p.volume = SV_MAX(p.volume, 0.f);
        p.sample_velocity = SV_MAX(p.sample_velocity, 0.f);
        p.range = SV_MAX(p.range, 0.f);
    }
    else
    {
        p.volume = 1.f;
        p.sample_velocity = 1.f;
        p.range = 10.f;
        p.position = v3_zero();
        p.velocity = v3_zero();
    }

    return p;
}

///////////////////////////////////////////////////////////// AUDIO ASSET ////////////////////////////////////////////////

static b8 asset_audio_load_file(void *asset, const char *filepath)
{
    Audio *audio = asset;

    if (!audio_load(audio, filepath))
    {
        return FALSE;
    }

    return TRUE;
}

static void asset_audio_free(void *asset)
{
    Audio *audio = asset;
    audio_destroy(audio);
}

static b8 asset_audio_reload_file(void *asset, const char *filepath)
{
    asset_audio_free(asset);
    return asset_audio_load_file(asset, filepath);
}

///////////////////////////// LISTENER ///////////////////////////////

void listener_update(const AudioProperties *props, v3 direction)
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

typedef struct
{
    u32 riff;
    u32 file_size;
    u32 wave;
} WaveHeader;

typedef struct
{
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

b8 audio_load(Audio *audio, const char *filepath)
{
    Deserializer s;
    u8 *data;
    u32 size;

    b8 res = TRUE;

    if (file_read_binary(FilepathType_Asset, filepath, &data, &size))
    {

        deserializer_begin_buffer(&s, data, size);

        WaveHeader header;
        deserializer_read(&s, &header, sizeof(WaveHeader));

        if (header.riff != RiffStr_RIFF || header.wave != RiffStr_WAVE || header.file_size != size - 8)
        {
            SV_LOG_ERROR("That's an invalid .wav format '%s'\n", filepath);
            goto corrupted;
        }

        WaveFmt *fmt;
        b8 read_fmt = FALSE;
        void *sample_data = NULL;
        u32 sample_bytes = 0;

        while (s.cursor < size)
        {

            u32 chunk_id;
            u32 chunk_size;

            deserialize_u32(&s, &chunk_id);
            deserialize_u32(&s, &chunk_size);

            u32 begin_offset = s.cursor;

            switch (chunk_id)
            {

            case RiffStr_fmt:
            {
                fmt = (WaveFmt *)(s.data + s.cursor);

                if (fmt->format_tag == 1)
                {

                    if (fmt->channel_count < 1 || fmt->channel_count > 2)
                    {
                        SV_LOG_ERROR("Invalid channel count %u in '%s'\n", fmt->channel_count, filepath);
                        break;
                    }

                    read_fmt = TRUE;
                }
                else
                {
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

        if (!read_fmt)
        {
            SV_LOG_ERROR("A valid fmt block is not found in '%s'\n", filepath);
            goto corrupted;
        }
        if (sample_bytes == 0)
        {
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

            if (audio->channel_count == 1)
            {
                audio->samples[0] = memory_allocate(per_channel_bytes);
                audio->samples[1] = NULL;

                i16 *src = (i16 *)sample_data;
                foreach (i, sample_count)
                {

                    audio->samples[0][i] = *src++;
                }
            }
            else if (audio->channel_count == 2)
            {
                u8 *mem = memory_allocate(per_channel_bytes * 2);
                audio->samples[0] = (i16 *)mem;
                mem += per_channel_bytes;
                audio->samples[1] = (i16 *)mem;

                i16 *src = (i16 *)sample_data;
                foreach (i, sample_count)
                {

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
    else
        res = FALSE;

    return res;
}

void audio_destroy(Audio *audio)
{
    if (audio == NULL)
        return;

    if (audio->samples[0])
        memory_free(audio->samples[0]);
}

inline void free_audio_instance(AudioInstance *inst)
{
    asset_decrement(inst->audio_asset);
    memory_zero(inst, sizeof(AudioInstance));
    sound->instance_free_count++;
    // TODO: Decrease instance_free_count
}

void audio_play(Asset audio_asset, const AudioProperties *props)
{
    AudioInstance *inst = NULL;

    mutex_lock(sound->mutex_instance);

    if (sound->instance_free_count)
    {

        foreach (i, sound->instance_count)
        {

            AudioInstance *instance = sound->instances + i;
            if (instance->audio_asset == 0)
            {
                inst = instance;
                break;
            }
        }

        if (inst != NULL)
            sound->instance_free_count--;

        assert(inst);
    }

    if (inst == NULL)
    {

        if (sound->instance_count >= AUDIO_INSTANCE_MAX)
        {
            SV_LOG_WARNING("Audio instances exceeded, the limit is %u\n", AUDIO_INSTANCE_MAX);
        }
        else
            inst = sound->instances + sound->instance_count++;
    }

    mutex_unlock(sound->mutex_instance);

    if (inst != NULL)
    {

        inst->props = validate_props(props);
        inst->begin_sample_index = sound->sample_index;
        inst->audio_asset = audio_asset;

        asset_increment(inst->audio_asset);
    }
}

inline void free_audio_source(u64 hash);
inline void music_free(Music *music);

void audio_stop()
{
    // Audio instances
    {
        mutex_lock(sound->mutex_instance);

        foreach (i, sound->instance_count)
        {

            AudioInstance *inst = sound->instances + i;
            if (inst->audio_asset != 0)
            {
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

        foreach (source_index, sound->source_count)
        {

            AudioSource *src = sound->sources + source_index;
            free_audio_source(src->hash);
        }

        audio_source_unlock();
    }

    // Music
    {
        music_lock();

        foreach (i, sound->music_count)
        {
            music_free(sound->music + i);
        }

        music_unlock();
    }
}

///////////////////////////////// AUDIO SOURCE //////////////////////////////////////////

// Add a valid asset to create a new one if doesn't exists
inline AudioSource *get_audio_source(u64 hash, Asset audio_asset)
{
    AudioSource *src = NULL;
    AudioSource *parent = NULL;

    AudioSource **source = sound->source_table + hash % AUDIO_SOURCE_TABLE_SIZE;

    if (*source)
    {

        src = *source;

        while (src->hash != hash)
        {

            parent = src;
            src = src->next;
            if (src == NULL)
            {
                break;
            }
        }
    }

    if (src == NULL && audio_asset)
    {

        // Create a new one
        {
            if (sound->free_sources)
            {
                src = sound->free_sources;
                sound->free_sources = src->next;
            }

            if (src == NULL)
            {
                if (sound->source_count >= AUDIO_SOURCE_MAX)
                {
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

        if (parent != NULL)
        {

            parent->next = src;
        }
        else
            *source = src;
    }

    return src;
}

inline void free_audio_source(u64 hash)
{
    AudioSource *src = NULL;
    AudioSource *parent = NULL;

    AudioSource **source = sound->source_table + hash % AUDIO_SOURCE_TABLE_SIZE;

    if (*source)
    {

        src = *source;

        while (src->hash != hash)
        {

            parent = src;
            src = src->next;
            if (src == NULL)
            {
                break;
            }
        }

        if (src != NULL)
        {

            if (parent != NULL)
            {
                parent->next = src->next;
            }
            else
            {
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

void audio_source_desc(u64 id, const AudioProperties *props)
{
    u64 hash = id;

    AudioSource *src = get_audio_source(hash, 0);

    if (src == NULL)
        return;

    src->props = validate_props(props);
}

void audio_source_continue(u64 id)
{
    u64 hash = id;
    AudioSource *src = get_audio_source(hash, 0);

    if (src == NULL)
        return;

    src->flags &= ~AudioSourceFlag_Stopped;
}

void audio_source_pause(u64 id)
{
    u64 hash = id;
    AudioSource *src = get_audio_source(hash, 0);

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
    AudioSource *src = get_audio_source(hash, 0);

    if (src == NULL)
        return FALSE;

    return src->audio_asset != 0 && !(src->flags & AudioSourceFlag_Stopped);
}

void audio_source_play(u64 id, Asset audio_asset, const AudioProperties *source_props, const AudioProperties *audio_props)
{
    u64 hash = id;
    AudioSource *src = get_audio_source(hash, audio_asset);

    // TODO: Play individual sounds

    if (src == NULL)
        return;

    src->props = validate_props(source_props);
    src->begin_sample_index = sound->sample_index;
}

/////////////////////////////////////////////////// MUSIC ////////////////////////////////////////////////////////

inline Music *music_get(u64 id, b8 create)
{
    if (id == 0)
        return NULL;

    // Find
    Music *music = NULL;
    foreach (i, sound->music_count)
    {
        if (sound->music[i].id == id)
        {
            music = sound->music + i;
            break;
        }
    }

    if (music == NULL && create)
    {
        if (sound->music_free_count)
        {
            foreach (i, sound->music_count)
            {
                if (sound->music[i].id == 0)
                {
                    music = sound->music + i;
                    sound->music_free_count--;
                    break;
                }
            }
        }

        if (music == NULL)
        {
            if (sound->music_count >= MUSIC_MAX)
            {
                SV_LOG_ERROR("Music playing limit is '%u'\n", MUSIC_MAX);
                return NULL;
            }

            music = sound->music + sound->music_count;
            sound->music_count++;
        }

        assert(music != NULL);

        memory_zero(music, sizeof(Music));

        music->id = id;
        music->fade_out_start = u32_max;
    }

    return music;
}

inline void music_free(Music *music)
{
    if (music->id == 0)
        return;

    if (music->audio_asset)
    {
        asset_unload(&music->audio_asset);
    }

    sound->music_free_count++;
    memory_zero(music, sizeof(Music));
}

void music_play(u64 id, Asset audio_asset, const AudioProperties *props)
{
    if (audio_asset == 0 || id == 0)
        return;

    Music *music = music_get(id, TRUE);
    if (music == NULL)
        return;

    music->audio_asset = audio_asset;
    music->begin_sample_index = sound->sample_index;
    music->props = validate_props(props);

    asset_increment(music->audio_asset);
}

void music_desc(u64 id, const AudioProperties *props)
{
    Music *music = music_get(id, FALSE);
    if (music == NULL)
        return;

    music->props = validate_props(props);
}

void music_continue(u64 id)
{
    // TODO
}

void music_pause(u64 id)
{
    // TODO
}

void music_stop(u64 id, f32 fade_out)
{
    Music *music = music_get(id, FALSE);
    if (music == NULL)
        return;

    if (fade_out > 0.0001f)
    {
        music->fade_out_start = sound->sample_index;
        music->fade_out_time = fade_out;
    }
    else
    {
        music_free(music);
    }
}

b8 music_is_playing(u64 id)
{
    Music *music = music_get(id, FALSE);
    return music != NULL;
}

void music_lock()
{
    mutex_lock(sound->mutex_music);
}

void music_unlock()
{
    mutex_unlock(sound->mutex_music);
}

///////////////////////////////////////////////////////// SOUND BUFFER UTILS ////////////////////////////////////////////

static v2 audio_sample(const Audio *audio, u32 sample_index, const i16 *sample0, const i16 *sample1, b8 *is_finished)
{
    *is_finished = FALSE;

    f32 ns0 = (f32)sample_index / (f32)sound->samples_per_second * (f32)audio->samples_per_second;
    f32 ns1 = (f32)(sample_index + 1) / (f32)sound->samples_per_second * (f32)audio->samples_per_second;

    u32 s0 = math_truncate_high(ns0 - EPSILON);
    u32 s1 = SV_MAX((u32)(ns1 - EPSILON), s0);

    // TODO: Check if the audio should be repeated
    b8 repeat = FALSE;

    if (!repeat)
    {
        if (s0 >= audio->sample_count)
        {
            *is_finished = TRUE;
            return v2_zero();
        }

        s1 = SV_MIN(s1, audio->sample_count - 1);
    }

    v2 value = v2_zero();

    f32 mult = 1.f / (f32)(s1 - s0 + 1);

    for (u32 s = s0; s <= s1; ++s)
    {
        u32 index = s % audio->sample_count;
        value.x += (f32)sample0[index] * mult;
        value.y += (f32)sample1[index] * mult;
    }

    return value;
}

static b8 write_audio(u32 begin_sample_index, Asset audio_asset, const AudioProperties *props, const AudioListener *listener, u32 samples_to_write)
{
    const Audio *audio = asset_get(audio_asset);

    // TODO:
    // - Variable velocity
    // - Use two loops, one for each channel

    if (audio == NULL)
        return TRUE;

    const i16 *sample0 = audio->samples[0];
    const i16 *sample1 = audio->samples[1];

    if (audio->channel_count == 1)
        sample1 = sample0;

    if (sample0 == NULL || sample1 == NULL)
        return FALSE;

    f32 v0 = props->volume;
    f32 v1 = props->volume;

    // 3D effect
    if ((listener->props.flags & AudioFlag_3D) && (props->flags & AudioFlag_3D))
    {

        v3 to = v3_sub(props->position, listener->props.position);
        f32 to_distance = v3_length(to);

        if (to_distance > 0.00001f)
        {

            v3 to_normalized = v3_div_scalar(to, to_distance);

            // Fade off
            {
                const f32 range = SV_MIN(props->range, listener->props.range);

                const f32 min_dist = 1.f;
                const f32 dist_range = SV_MAX(range - min_dist, 0.f);

                if (dist_range > 0.00001f)
                {

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

    u32 sample_index = sound->sample_index - begin_sample_index;

    b8 is_finished = FALSE;

    foreach (i, samples_to_write)
    {
        v2 value = audio_sample(audio, sample_index + i, sample0, sample1, &is_finished);

        if (is_finished)
            break;

        sound->samples[i * 2 + 0] += value.x * v0;
        sound->samples[i * 2 + 1] += value.y * v1;
    }

    return !is_finished;
}

static i32 thread_main(void *_data)
{
    const u32 updates_per_second = 500;

    while (!sound->close_request)
    {
        f64 start_time = timer_now();

        u32 samples_to_write, sample_offset;
        sound_compute_sample_range(sound->sample_index, &samples_to_write, &sample_offset);

        if (samples_to_write)
        {
            mutex_lock(sound->mutex_listener);
            AudioListener listener = sound->listener;
            mutex_unlock(sound->mutex_listener);

            // Clear buffer

            foreach (i, samples_to_write)
            {
                sound->samples[i * 2 + 0] = 0.f;
                sound->samples[i * 2 + 1] = 0.f;
            }

            // Append audio instances
            {
                mutex_lock(sound->mutex_instance);

                foreach (instance_index, sound->instance_count)
                {
                    AudioInstance *inst = sound->instances + instance_index;

                    if (!write_audio(inst->begin_sample_index, inst->audio_asset, &inst->props, &listener, samples_to_write))
                    {
                        free_audio_instance(inst);
                    }
                }

                mutex_unlock(sound->mutex_instance);
            }

            // Append audio sources
            {
                audio_source_lock();

                foreach (source_index, sound->source_count)
                {
                    AudioSource *src = sound->sources + source_index;

                    if (!write_audio(src->begin_sample_index, src->audio_asset, &src->props, &listener, samples_to_write))
                    {
                        free_audio_source(src->hash);
                    }
                }

                audio_source_unlock();
            }

            // Append music
            {
                music_lock();

                foreach (music_index, sound->music_count)
                {
                    Music *music = sound->music + music_index;

                    if (music->id == 0)
                        continue;

                    AudioProperties props = music->props;

                    b8 free = FALSE;

                    if (music->fade_out_start < sound->sample_index)
                    {
                        f32 time = (f32)(sound->sample_index - music->fade_out_start) / (f32)sound->samples_per_second;
                        time /= music->fade_out_time;

                        free = time > 0.999f;

                        // TODO: Adjust curve
                        time = math_fade_inout(1.f - math_clamp01(time), 3.0f);

                        props.volume = time;
                    }

                    if (!write_audio(music->begin_sample_index, music->audio_asset, &props, &listener, samples_to_write) || free)
                    {
                        music_free(music);
                    }
                }

                music_unlock();
            }

            sound_fill_buffer(sound->samples, samples_to_write, sample_offset);

            sound->sample_index += samples_to_write;
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

    sound->samples_per_second = samples_per_second;

    SV_CHECK(sound_platform_initialize(samples_per_second));

    // Register audio asset
    {
        AssetTypeDesc desc;
        const char *extensions[10];
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
    sound->mutex_music = mutex_create();
    sound->mutex_listener = mutex_create();

    // Run thread
    sound->thread = thread_create(thread_main, NULL);

    if (sound->thread == 0)
    {
        SV_LOG_ERROR("Can't create the sound thread\n");
        return FALSE;
    }

    sound_configure_thread(sound->thread);

    return TRUE;
}

void sound_close()
{
    if (sound)
    {
        // TODO
        sound->close_request = TRUE;

        thread_wait(sound->thread);

        audio_stop();

        mutex_destroy(sound->mutex_source);
        mutex_destroy(sound->mutex_instance);
        mutex_destroy(sound->mutex_music);
        mutex_destroy(sound->mutex_listener);

        sound_platform_close();

        memory_free(sound->samples);
        memory_free(sound);
    }
}