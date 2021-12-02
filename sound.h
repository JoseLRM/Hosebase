#pragma once

#include "Hosebase/asset_system.h"

typedef struct {
	i16* samples[2];
	u32 sample_count;
	u32 channel_count;
	u32 samples_per_second;
} Audio;

typedef struct {
	f32 volume;
	f32 velocity;
	u64 flags;
} AudioProperties;

void audio_source_play_desc(u64 id, Asset audio_asset, const AudioProperties* props);
void audio_source_update_properties(u64 id, const AudioProperties* props);
void audio_source_continue(u64 id);
void audio_source_pause(u64 id);

b8 audio_load(Audio* audio, const char* filepath);
void audio_destroy(Audio* audio);

b8 _sound_initialize(u32 samples_per_second);
void _sound_close();