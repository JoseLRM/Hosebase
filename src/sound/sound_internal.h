#pragma once

#include "Hosebase/sound.h"
#include "Hosebase/platform.h"

b8 sound_platform_initialize(u32 samples_per_second);
void sound_platform_close();

void sound_configure_thread(Thread thread);

void sound_compute_sample_range(u32 sample_index, u32* sample_count, u32* offset);
void sound_fill_buffer(f32* samples, u32 sample_count, u32 offset);