#pragma once

#include "hosebase/defines.h"

#if SV_GRAPHICS

#include "Hosebase/font.h"
#include "Hosebase/text_processing.h"

#define DrawTextFlag_LineCentred SV_BIT(0)
#define DrawTextFlag_BottomTop SV_BIT(1)

typedef struct {

	GPUImage* render_target;

	u64 flags;
	const void* text;
	Font* font;
	u32 max_lines;

	f32 aspect;
	TextAlignment alignment;
	f32 font_size;
	Mat4 transform_matrix;

	TextContext* context;

	Color text_default_color;
	Color text_selected_color;
	Color selection_color;
	Color cursor_color;

} DrawTextDesc;

void draw_text(const DrawTextDesc* desc, CommandList cmd);

b8 render_utils_initialize();
void render_utils_close();

#endif