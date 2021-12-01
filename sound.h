#pragma once

#include "defines.h"

typedef struct {
	i16* samples[2];
	u32 sample_count;
	u32 channel_count;
	u32 samples_per_second;
} Audio;

b8 audio_load(Audio* audio, const char* filepath);
void audio_destroy(Audio* audio);

void audio_play();

b8 _sound_initialize(u32 samples_per_second, u32 buffer_size);
void _sound_close();