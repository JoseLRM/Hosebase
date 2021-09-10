#pragma once

#include "Hosebase/os.h"
#include "Hosebase/graphics.h"
#include "Hosebase/input.h"
#include "Hosebase/serialize.h"
#include "Hosebase/font.h"

// TODO: Optional

#include "Hosebase/imrend.h"

typedef struct {
	OSInitializeDesc os;
	GraphicsInitializeDesc graphics;
} HosebaseInitializeDesc;

inline b8 hosebase_initialize(const HosebaseInitializeDesc* desc)
{
	core.frame_count = 0u;
	core.delta_time = 0.f;
	core.time_step = 1.f;
	
	if (!_os_initialize(&desc->os)) {
		print("Can't initialize os layer\n");
		return FALSE;
	}

	if (!_input_initialize()) {
		print("Can't initialize input system\n");
		return FALSE;
	}

	if (!_graphics_initialize(&desc->graphics)) {
		print("Can't initialize graphics API\n");
		return FALSE;
	}

	if (!imrend_initialize()) {
		print("Can't initialize imrend\n");
		return FALSE;
	}

	return TRUE;
}

inline void hosebase_close()
{
	imrend_close();
	_graphics_close();
	_input_close();
	_os_close();
}

inline b8 hosebase_frame_begin()
{
	// Compute delta time
	{
		static f64 last = 0.0;
		f64 now = timer_now();
		
		core.delta_time = (f32)(now - last);
		last = now;
	}
	
	_input_update();
	if (!_os_recive_input()) return FALSE; // Close request

	_graphics_begin();

	return TRUE;
}

inline void hosebase_frame_end()
{
	_graphics_end();

	++core.frame_count;
}
