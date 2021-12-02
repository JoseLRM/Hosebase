#pragma once

#include "Hosebase/asset_system.h"

typedef struct {
	i16* samples[2];
	u32 sample_count;
	u32 channel_count;
	u32 samples_per_second;
} Audio;

typedef struct {
	Asset audio_asset;
	f32 volume;
	f32 velocity;
} AudioDesc;

b8 audio_load(Audio* audio, const char* filepath);
void audio_destroy(Audio* audio);

void audio_play_desc(u64 id, const AudioDesc* desc);

b8 _sound_initialize(u32 samples_per_second);
void _sound_close();