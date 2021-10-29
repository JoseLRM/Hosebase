#pragma once

#include "Hosebase/graphics.h"

typedef enum {
	GuiUnit_Relative,
	GuiUnit_Pixel,
	GuiUnit_Aspect,
} GuiUnit;

typedef enum {
	GuiCoordAlign_Center,
	GuiCoordAlign_Left,
	GuiCoordAlign_Right,
	GuiCoordAlign_InverseCenter,
	GuiCoordAlign_InverseLeft,
	GuiCoordAlign_InverseRight,
} GuiCoordAlign;

typedef struct {
	f32 value;
	GuiUnit unit;
	GuiCoordAlign align;
} GuiCoord;

typedef struct {
	f32 value;
	GuiUnit unit;
} GuiDimension;

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
	GuiUnit width_unit;
	f32 height;
	GuiUnit height_unit;
	f32 margin;
	GuiUnit margin_unit;
	f32 padding;
	GuiUnit padding_unit;
} GuiStackLayoutData;

GuiStackLayoutData gui_stack_layout_get_data();
void gui_stack_layout_update(GuiStackLayoutData data);
void gui_stack_layout_update_size(f32 width, GuiUnit width_unit, f32 height, GuiUnit height_unit);

typedef struct {

	GuiCoord x;
	GuiCoord y;
	GuiDimension width;
	GuiDimension height;

} GuiFreeLayoutData;

GuiFreeLayoutData gui_free_layout_get_data();
void gui_free_layout_update(GuiFreeLayoutData data);
void gui_free_layout_update_x(f32 value, GuiUnit unit, GuiCoordAlign align);
void gui_free_layout_update_y(f32 value, GuiUnit unit, GuiCoordAlign align);
void gui_free_layout_update_width(f32 value, GuiUnit unit);
void gui_free_layout_update_height(f32 value, GuiUnit unit);
