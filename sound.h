#pragma once

#include "Hosebase/asset_system.h"

SV_BEGIN_C_HEADER

typedef struct {
	i16* samples[2];
	u32 sample_count;
	u32 channel_count;
	u32 samples_per_second;
} Audio;

#define AudioFlag_3D SV_BIT(0)

typedef struct {
	f32 volume;
	f32 sample_velocity;
	f32 range;
	u64 flags;
	v3 position;
	v3 velocity;
} AudioProperties;

// LISTENER

void listener_update(const AudioProperties* props, v3 direction);

// AUDIO

b8 audio_load(Audio* audio, const char* filepath);
void audio_destroy(Audio* audio);

void audio_play(Asset audio_asset, const AudioProperties* props);
void audio_stop();

// AUDIO SOURCE

// Updates the audio source properties
void audio_source_desc(u64 id, const AudioProperties* props);

void audio_source_continue(u64 id);
void audio_source_pause(u64 id);
void audio_source_kill(u64 id);

b8 audio_source_is_playing(u64 id);

// Synchronization calls. Remember that if the sound is locked too much time will produce some artefacts
void audio_source_lock();
void audio_source_unlock();

// Play a audio with specified source_props that affects the entire audio source and audio_props that only affects this specific source
void audio_source_play(u64 id, Asset audio_asset, const AudioProperties* source_props, const AudioProperties* audio_props);

// MUSIC

void music_play(u64 id, Asset audio_asset, const AudioProperties* props);
void music_desc(u64 id, const AudioProperties* props);
void music_continue(u64 id);
void music_pause(u64 id);
void music_stop(u64 id);

// Internal

b8 _sound_initialize(u32 samples_per_second);
void _sound_close();

SV_END_C_HEADER