#pragma once

#include "Hosebase/graphics.h"

SV_BEGIN_C_HEADER

// TODO: Dynamic
#define _GUI_PARENT_WIDGET_BUFFER_SIZE (1500 * 30)

#define _GUI_LAYOUT_DATA_SIZE 500
#define _GUI_LAYOUT_STACK_SIZE (500 * 30)

#define _GUI_WIDGET_STACK_SIZE (500 * 30)

typedef enum {
	GuiUnit_Relative,
	GuiUnit_Pixel,
	GuiUnit_Aspect,
} GuiUnit;

typedef enum {
	GuiCoordAlign_Center,
	GuiCoordAlign_Left,
	GuiCoordAlign_Right,
	GuiCoordAlign_Bottom,
	GuiCoordAlign_Top,
	GuiCoordAlign_InverseCenter,
	GuiCoordAlign_InverseLeft,
	GuiCoordAlign_InverseRight,
	GuiCoordAlign_InverseBottom,
	GuiCoordAlign_InverseTop,
} GuiCoordAlign;

typedef struct {
	f32 value;
	GuiUnit unit;
	GuiCoordAlign align;
} GuiCoord;

typedef struct {
	u32 type;
	u8 data[_GUI_LAYOUT_DATA_SIZE];

	u8 stack[_GUI_LAYOUT_STACK_SIZE];
	u32 stack_size;
} GuiLayout;

typedef struct {
	u64 id;
	u64 flags;
	u32 type;
	v4 bounds;
} GuiWidget;

typedef struct GuiParent GuiParent;

struct GuiParent {
	u64 id;
	v4 bounds;

	v4 widget_bounds;
	u8 widget_buffer[_GUI_PARENT_WIDGET_BUFFER_SIZE];
	u32 widget_buffer_size;
	u32 widget_count;

	// This values are used to know the last widget found.
	// In the 99% of the cases the widget that is trying to find it's the next one.
	u64 last_widget_search_byte;
	u32 last_widget_search_i;

	GuiLayout layout;

	GuiParent* parent;
	// TODO: Dynamic
	GuiParent* childs[1000];
	u32 child_count;

	struct {
		GPUImage* image;
		v4 texcoord;
		Color color;
	} background;

	f32 vrange;

	u32 state;
};

typedef struct {
	f32 value;
	GuiUnit unit;
} GuiDimension;

typedef struct {
	u64 parent_id;
	GuiParent* parent;
	u64 id;
	u32 type;
	GuiWidget* widget;
	u32 action;
} GuiFocus;

b8 gui_initialize();
void gui_close();

void gui_begin(const char* layout, Font* default_font);
void gui_end();
void gui_draw(GPUImage* image, CommandList cmd);

void gui_push_id(u64 id);
void gui_pop_id(u32 count);

inline void gui_push_string_id(const char* str)
{
	gui_push_id((u64)str);
}

Font* gui_font();

void gui_free_focus();
void gui_set_focus(GuiWidget* widget, u64 parent_id, u32 action);

GuiFocus gui_get_focus();
b8 gui_has_focus();

// Parents

void gui_parent_begin(const char* name, const char* layout);
void gui_parent_end();

void gui_set_background(GPUImage* image, v4 texcoord, Color color);

// Widget utils

#define gui_write(data) gui_write_(&data, sizeof(data))

void gui_write_(const void* data, u32 size);
void gui_write_text(const char* text);
u64 gui_write_widget(u32 type, u64 flags, u64 id);
b8 imgui_write_layout_update(u32 type);

#define gui_read(it, data) gui_read_(&it, &data, sizeof(data))
#define gui_read_text(it, text) gui_read_text_(&it, &text)
#define gui_read_buffer(it, size) gui_read_buffer_(&it, size)

void gui_read_(u8** it_, void* data, u32 size);
void gui_read_text_(u8** it_, const char** text);
const void* gui_read_buffer_(u8** it_, u32 size);

v2 gui_resolution();
v2 gui_pixel_size();
v2 gui_mouse_position();

f32 gui_aspect();

b8 gui_scrolling();

void gui_draw_bounds(v4 bounds, Color color);
void gui_draw_sprite(v4 bounds, Color color, GPUImage* image, v4 tc);
void gui_draw_text(const char* text, v4 bounds, TextAlignment alignment);

b8 gui_mouse_in_bounds(v4 bounds);
b8 gui_bounds_inside(v4 bounds);
b8 gui_bounds_inside_bounds(v4 child, v4 parent);

GuiWidget* gui_find_widget(u32 type, u64 id, GuiParent* parent);
GuiParent* gui_find_parent(u64 parent_id);
GuiParent* gui_current_parent();

f32 gui_compute_coord(GuiCoord coord, b8 vertical, f32 dimension, f32 parent_dimension);
f32 gui_compute_dimension(GuiDimension dimension, b8 vertical, f32 parent_dimension);

// Layout property stack

void gui_layout_push_ex(const char* name, const void* data, u16 size);
void gui_layout_set_ex(const char* name, const void* data, u16 size);
void gui_layout_pop(u32 count);

// Widget property stack

void gui_widget_push_ex(u32 widget_id, const char* name, const void* data, u16 size);
void gui_widget_set_ex(u32 widget_id, const char* name, const void* data, u16 size);
void gui_widget_pop(u32 widget_id, u32 count);

// Registers

typedef struct {
	const char* name;
	u8* (*read_fn)(GuiWidget* widget, u8* it);
	void (*update_fn)(GuiParent* parent, GuiWidget* widget, b8 has_focus);
	void (*draw_fn)(GuiWidget* widget);
	b8(*property_read_fn)(u32 property, u8* it, u32 size, u8* pop_data);
	u16(*property_id_fn)(const char* name);
	u32 size;
} GuiRegisterWidgetDesc;

u32 gui_register_widget(const GuiRegisterWidgetDesc* desc);

typedef struct {
	const char* name;
	void(*initialize_fn)(GuiParent* parent);
	v4(*compute_bounds_fn)(GuiParent* parent);
	b8(*property_read_fn)(GuiParent* parent, u32 property, u8* it, u32 size, u8* pop_data);
	u16(*property_id_fn)(const char* name);
} GuiRegisterLayoutDesc;

u32 gui_register_layout(const GuiRegisterLayoutDesc* desc);

SV_END_C_HEADER