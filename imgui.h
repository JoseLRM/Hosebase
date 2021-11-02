#pragma once

#include "Hosebase/graphics.h"

#define GUI_PARENT_WIDGET_BUFFER_SIZE (500 * 30)
#define GUI_LAYOUT_DATA_SIZE 500

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
	u32 type;
	u8 data[GUI_LAYOUT_DATA_SIZE];
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
	u8 widget_buffer[GUI_PARENT_WIDGET_BUFFER_SIZE];
	u32 widget_buffer_size;
	u32 widget_count;
	GuiParent* parent;
	GuiLayout layout;

	struct {
		GPUImage* image;
		v4 texcoord;
		Color color;
	} background;
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
} GuiFocus;

b8 gui_initialize();
void gui_close();

void gui_begin(const char* layout, Font* default_font);
void gui_end();
void gui_draw(GPUImage* image, CommandList cmd);

void gui_push_id(u64 id);
void gui_pop_id(u32 count);

void gui_free_focus();
void gui_set_focus(GuiWidget* widget, u64 parent_id);

GuiFocus gui_get_focus();

typedef struct {
	const char* name;
	u8*(*read_fn)(GuiWidget* widget, u8* it);
	u8*(*update_fn)(GuiParent* parent, GuiWidget* widget, b8 has_focus);
	u8*(*draw_fn)(GuiWidget* widget);
	u32 size;
} GuiRegisterWidgetDesc;

u32 gui_register_widget(const GuiRegisterWidgetDesc* desc);

// Parents

void gui_begin_parent(const char* layout);
void gui_end_parent();

void gui_set_background(GPUImage* image, v4 texcoord, Color color);

// Widget utils

#define gui_write(data) gui_write_(&data, sizeof(data))

void gui_write_(const void* data, u32 size);
void gui_write_text(const char* text);
u64 gui_write_widget(u32 type, u64 flags, u64 id);
b8 imgui_write_layout_update(u32 type);

#define gui_read(it, data) gui_read_(&it, &data, sizeof(data))
#define gui_read_text(it, text) gui_read_text_(&it, &text)

void gui_read_(u8** it_, void* data, u32 size);
void gui_read_text_(u8** it_, const char** text);

void gui_draw_bounds(v4 bounds, Color color);
void gui_draw_sprite(v4 bounds, Color color, GPUImage* image, v4 tc);
void gui_draw_text(const char* text, v4 bounds, TextAlignment alignment);

b8 gui_mouse_in_bounds(v4 bounds);

GuiWidget* gui_find_widget(u32 type, u64 id, GuiParent* parent);
GuiParent* gui_find_parent(u64 parent_id);
GuiParent* gui_current_parent();

f32 gui_compute_coord(GuiCoord coord, b8 vertical, f32 dimension, f32 parent_dimension);
f32 gui_compute_dimension(GuiDimension dimension, b8 vertical, f32 parent_dimension);

// Default widgets

typedef void(*GuiDrawableFn)(GuiWidget* widget);

b8 gui_button(const char* text, u64 flags);
void gui_drawable(GuiDrawableFn fn, u64 flags);

// Default layouts

typedef struct {
	GuiDimension width;
	GuiDimension height;
	GuiDimension margin;
	GuiDimension padding;
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
