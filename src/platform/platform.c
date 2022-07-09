#include "Hosebase/platform.h"

typedef struct {
	int a; // TODO:
} Platform;

static Platform* platform;

//////////// INTERNAL DEFINES /////////////

b8 os_initialize(const PlatformInitializeDesc *desc);
void os_close();

/////////////////////////////////////////

b8 platform_initialize(const PlatformInitializeDesc *desc)
{
    platform = memory_allocate(sizeof(Platform));
    return os_initialize(desc);
}

void platform_close()
{
    if (platform != NULL)
    {
        memory_free(platform);
    }

    os_close();
}

b8 path_is_absolute(const char *path)
{
	if (path == NULL)
		return FALSE;

	u32 size = string_size(path);
	if (size < 2u)
		return FALSE;
	return path[1] == ':';
}

/////////////////// WINDOW /////////////////////

f32 window_aspect()
{
	v2_u32 size = window_size();
	return (f32)size.x / (f32)size.y;
}