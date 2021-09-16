#include "src/networking.c"

#if SV_PLATFORM_WINDOWS
#include "src/win64.c"
#endif

#include "src/memory_manager.c"
#include "src/serialize.c"
#include "src/input.c"
#include "src/graphics.c"
#include "src/graphics_shader.c"
#include "src/imrend.c"
#include "src/font.c"

CoreData core;
