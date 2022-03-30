#include "src/networking/networking.c"
#include "src/networking/networking_server.c"
#include "src/networking/networking_client.c"

#if SV_PLATFORM_WINDOWS
#include "src/win64_sound.c"
#include "src/win64.c"
#endif

#include "src/memory_manager.c"
#include "src/serialize.c"
#include "src/input.c"
#include "src/event_system.c"
#include "src/asset_system.c"
#include "src/graphics.c"
#include "src/graphics_shader.c"
#include "src/imrend.c"
#include "src/imgui.c"
#include "src/font.c"
#include "src/text_processing.c"
#include "src/render_utils.c"
#include "src/profiler.c"

CoreData core;
