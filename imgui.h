#pragma once

#include "Hosebase/graphics.h"

b8 gui_initialize();
void gui_close();

void gui_begin(const char* layout, Font* default_font);
void gui_end();
void gui_draw(GPUImage* image, CommandList cmd);

void gui_push_id(u64 id);
void gui_pop_id(u32 count);

// Parents

void gui_begin_parent(const char* layout);
void gui_end_parent();

// Widget utils

void gui_draw_bounds(v4 bounds, Color color);
void gui_draw_sprite(v4 bounds, Color color, GPUImage* image, v4 tc);
void gui_draw_text(const char* text, v4 bounds, TextAlignment alignment);

b8 gui_mouse_in_bounds(v4 bounds);

// Default widgets

b8 gui_button(const char* text, u64 flags);

// Default layouts

typedef struct {
	f32 width;
	f32 height;
	f32 margin;
	f32 padding;
} GuiStackLayoutData;

GuiStackLayoutData gui_stack_layout_get_data();
void gui_stack_layout_update(GuiStackLayoutData data);
void gui_stack_layout_update_size(f32 width, f32 height);
