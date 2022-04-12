#pragma once

#include "Hosebase/os.h"
#include "Hosebase/sound.h"
#include "Hosebase/graphics.h"
#include "Hosebase/input.h"
#include "Hosebase/event_system.h"
#include "Hosebase/asset_system.h"
#include "Hosebase/serialize.h"
#include "Hosebase/font.h"
#include "Hosebase/networking.h"
#include "Hosebase/profiler.h"
#include "Hosebase/render_utils.h"
#include "Hosebase/imgui.h"

typedef struct {
	OSInitializeDesc os;

#if SV_GRAPHICS
	GraphicsInitializeDesc graphics;
#endif

	struct {
		b8 hot_reload;
	} asset;

} HosebaseInitializeDesc;

inline b8 hosebase_initialize(const HosebaseInitializeDesc* desc)
{
	core.frame_count = 0u;
	core.delta_time = 0.f;
	core.time_step = 1.f;

	if (!_event_initialize()) {
		SV_LOG_ERROR("Can't initialize event system\n");
	}

	if (!_asset_initialize(desc->asset.hot_reload)) {
		SV_LOG_ERROR("Can't initialize asset system\n");
	}
	
	if (!_os_initialize(&desc->os)) {
		SV_LOG_ERROR("Can't initialize os layer\n");
		return FALSE;
	}

#if SV_SLOW
	_profiler_initialize();
#endif

	if (!_sound_initialize(44800)) {
		SV_LOG_ERROR("Can't initialize audio system");
	}

	if (!_input_initialize()) {
		SV_LOG_ERROR("Can't initialize input system\n");
		return FALSE;
	}

#if SV_NETWORKING

	if (!_net_initialize()) {
		SV_LOG_ERROR("Can't initialize networking\n");
		return FALSE;
	}
	
#endif

#if SV_GRAPHICS

	if (!_graphics_initialize(&desc->graphics)) {
		SV_LOG_ERROR("Can't initialize graphics API\n");
		return FALSE;
	}

	if (!render_utils_initialize()) {
		SV_LOG_ERROR("Can't initialize render utils\n");
		return FALSE;
	}

	if (!gui_initialize()) {
		SV_LOG_ERROR("Can't initialize ImGui\n");
		return FALSE;
	}

#endif

	return TRUE;
}

inline void hosebase_close()
{
#if SV_GRAPHICS
	gui_close();
	render_utils_close();
#endif

	asset_free_unused();

#if SV_GRAPHICS
	_graphics_close();
#endif

#if SV_NETWORKING
	_net_close();
#endif

	_sound_close();
	
	_input_close();

#if SV_SLOW
	_profiler_close();
#endif

	_os_close();
	
	_asset_close();
	_event_close();
}

inline b8 hosebase_frame_begin()
{
	profiler_function_begin();

	// Compute delta time
	{
		static f64 fps_cache = 0.0;
		f64 now = timer_now();
		
		core.delta_time = SV_MIN((f32)(now - core.last_update), 0.15f);
		core.last_update = now;

		fps_cache = fps_cache * 0.9f + core.delta_time * 0.1f;
		core.FPS = (u32)(1.0 / fps_cache);
	}

#if SV_SLOW
	_profiler_reset();
#endif
	
	_input_update();
	if (!_os_recive_input()) return FALSE; // Close request

	_asset_update();

	profiler_function_end();

	return TRUE;
}

inline void hosebase_frame_end()
{
	++core.frame_count;
}
