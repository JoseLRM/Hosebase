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

	if (!_sound_initialize(4800)) {
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

#endif

	if (!gui_initialize()) {
		SV_LOG_ERROR("Can't initialize ImGui\n");
		return FALSE;
	}

	return TRUE;
}

inline void hosebase_close()
{
	gui_close();
	
#if SV_GRAPHICS
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
	_os_close();
	_asset_close();
	_event_close();
}

inline b8 hosebase_frame_begin()
{
	// Compute delta time
	{
		static f64 last = 0.0;
		f64 now = timer_now();
		
		core.delta_time = SV_MIN((f32)(now - last), 0.3f);
		last = now;
	}
	
	_input_update();
	if (!_os_recive_input()) return FALSE; // Close request

#if SV_GRAPHICS
	_graphics_begin();
#endif

	_asset_update();

	return TRUE;
}

inline void hosebase_frame_end()
{
#if SV_GRAPHICS
	_graphics_end();
#endif
	_sound_update();

	++core.frame_count;
}
