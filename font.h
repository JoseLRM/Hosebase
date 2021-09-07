#pragma once

#include "Hosebase/graphics.h"

typedef struct {
	v4  texcoord;
	f32 advance;
	f32 xoff, yoff;
	f32 w, h;
	f32 left_side_bearing;
} Glyph;

typedef enum {
	FontFlag_None,
	FontFlag_Monospaced,
} FontFlag;
typedef u32 FontFlags;

#define FONT_CHAR_COUNT ((u32)('~') + 1u)

typedef struct {
	Glyph* glyphs;
	GPUImage* image;
	f32 vertical_offset;
	f32 pixel_height;
} Font;

inline Glyph* font_get(Font* font, u32 index)
{
	if (index < FONT_CHAR_COUNT)
		return font->glyphs + index;
	return NULL;
}

typedef enum {
		TextAlignment_Left,
		TextAlignment_Center,
		TextAlignment_Right,
		TextAlignment_Justified,
} TextAlignment;

b8   font_create(Font* font, const char* filepath, f32 pixel_height, FontFlags flags);
void font_destroy(Font* font);
