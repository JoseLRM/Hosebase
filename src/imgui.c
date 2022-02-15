#include "Hosebase/imgui.h"

#define PARENTS_MAX 100

typedef enum {
	GuiHeader_Widget,
	GuiHeader_BeginParent,
	GuiHeader_EndParent,
	GuiHeader_SetBackground,
	GuiHeader_LayoutPush,
	GuiHeader_LayoutSet,
	GuiHeader_LayoutPop,
	GuiHeader_WidgetPush,
	GuiHeader_WidgetPop,
} GuiHeader;

typedef struct {
	char name[NAME_SIZE];
	u8*(*read_fn)(GuiWidget* widget, u8* it);
	void(*update_fn)(GuiParent* parent, GuiWidget* widget, b8 has_focus);
	void(*draw_fn)(GuiWidget* widget);
	b8(*property_read_fn)(u32 property, u8* it, u32 size, u8* pop_data);
	u16(*property_id_fn)(const char* name);
	u32 size;

	u8 stack[_GUI_WIDGET_STACK_SIZE];
	u32 stack_size;

} GuiWidgetRegister;

typedef struct {
	void(*initialize_fn)(GuiParent* parent);
	v4(*compute_bounds_fn)(GuiParent* parent);
	b8(*property_read_fn)(GuiParent* parent, u32 property, u8* it, u32 size, u8* pop_data);
	u16(*property_id_fn)(const char* name);
	char name[NAME_SIZE];
} GuiLayoutRegister;

typedef struct {

	Buffer buffer;
	
	GuiParent parents[PARENTS_MAX];
	u32 parent_count;

	GuiParent root;

	GuiParent* parent_stack[PARENTS_MAX];
	u32 parent_stack_count;

	DynamicArray(u64) id_stack;
	u64 current_id;

	GuiFocus focus;

	CommandList cmd;
	GPUImage* image;
	Font* default_font;
	f32 aspect;
	v2 resolution;
	v2 pixel;
	v2 mouse_position;

	GuiWidgetRegister widget_registers[100];
	u32 widget_register_count;

	GuiLayoutRegister layout_registers[100];
	u32 layout_register_count;

	struct {
		u32 layout_free;
	} register_ids;
	
} GUI;

static GUI* gui;

/////////////////////////////// WIDGET UTILS ///////////////////////////

static u32 gui_widget_size(u32 type)
{
	if (type >= gui->widget_register_count) return 0;
	return gui->widget_registers[type].size;
}

static u8* gui_widget_read(GuiWidget* widget, u8* it)
{
	u32 type = widget->type;
	if (type >= gui->widget_register_count) return it;
	return gui->widget_registers[type].read_fn(widget, it);
}

static void gui_widget_update(GuiParent* parent, GuiWidget* widget, b8 has_focus)
{
	u32 type = widget->type;
	if (type >= gui->widget_register_count) return;
	if (gui->widget_registers[type].update_fn)
		gui->widget_registers[type].update_fn(parent, widget, has_focus);
}

static void gui_widget_draw(GuiWidget* widget)
{
	u32 type = widget->type;
	if (type >= gui->widget_register_count) return;
	if (gui->widget_registers[type].draw_fn)
		gui->widget_registers[type].draw_fn(widget);
}

static b8 gui_widget_property_read(u32 type, u32 property, u8* it, u32 size, u8* pop_data)
{
	if (type >= gui->widget_register_count) return FALSE;
	return gui->widget_registers[type].property_read_fn(property, it, size, pop_data);
}

static u16 gui_widget_property_id(u32 type, const char* name)
{
	if (type >= gui->widget_register_count) return 0;
	return gui->widget_registers[type].property_id_fn(name);
}

//////////////////////////////// LAYOUT UTILS //////////////////////

static void gui_initialize_layout(GuiParent* parent)
{
	memory_zero(&parent->layout.data, _GUI_LAYOUT_DATA_SIZE);
	u32 type = parent->layout.type;

	if (type >= gui->layout_register_count) return;
	gui->layout_registers[type].initialize_fn(parent);
}

static v4 gui_compute_bounds(GuiParent* parent)
{
	u32 type = parent->layout.type;

	if (type >= gui->layout_register_count) return v4_zero();
	return gui->layout_registers[type].compute_bounds_fn(parent);
}

static b8 gui_layout_property_read(GuiParent* parent, u32 property, u8* it, u32 size, u8* pop_data)
{
	u32 type = parent->layout.type;

	if (type >= gui->layout_register_count) return FALSE;
	return gui->layout_registers[type].property_read_fn(parent, property, it, size, pop_data);
}

static u32 gui_layout_property_id(GuiParent* parent, const char* name)
{
	u32 type = parent->layout.type;

	if (type >= gui->layout_register_count) return 0;
	return gui->layout_registers[type].property_id_fn(name);
}

static u32 gui_find_layout_index(const char* name)
{
	// TODO: Optimize
	foreach(i, gui->layout_register_count) {
		
		if (string_equals(gui->layout_registers[i].name, name))
			return i;
	}
	return u32_max;
}

/////////////////////////// PARENT UTILS //////////////////////////////

static void gui_push_parent(GuiParent* parent)
{
	gui->parent_stack[gui->parent_stack_count++] = parent;
}

static void gui_pop_parent()
{
	if (gui->parent_stack_count != 0) {
		gui->parent_stack_count--;
		gui->parent_stack[gui->parent_stack_count] = NULL;
	}
	else assert_title(FALSE, "Parent stack error");
}

static void adjust_widget_bounds(GuiParent* parent)
{
	v4 pb = parent->widget_bounds;
	f32 inv_height = 1.f / gui->resolution.y;
	
	u8* it = parent->widget_buffer;

	foreach(i, parent->widget_count) {

		GuiWidget* w = (GuiWidget*)it;

		w->bounds.x = (w->bounds.x * pb.z) + pb.x - (pb.z * 0.5f);
		w->bounds.y = (w->bounds.y * pb.w) + pb.y - (pb.w * 0.5f);
		w->bounds.z *= pb.z;
		w->bounds.w *= pb.w;

		it += sizeof(GuiWidget) + gui_widget_size(w->type);
	}
}

static void update_parent(GuiParent* parent)
{
	u8* it = parent->widget_buffer;

	foreach(i, parent->widget_count) {

		GuiWidget* widget = (GuiWidget*)it;

		if (widget->type == gui->focus.type && widget->id == gui->focus.id)
			continue;

		gui_widget_update(parent, widget, FALSE);
		it += sizeof(GuiWidget) + gui_widget_size(widget->type);
	}
}

static void draw_parent(GuiParent* parent)
{
	v4 b = parent->widget_bounds;
	imrend_push_scissor(b.x, b.y, b.z, b.w, FALSE, gui->cmd);

	// Background
	{
		gui_draw_sprite(b, parent->background.color, parent->background.image, parent->background.texcoord);
	}

	// Draw childs
	foreach(i, gui->parent_count) {

		GuiParent* child = gui->parents + i;

		if (child->parent == parent)
			draw_parent(child);
	}

	// Draw widgets
	{
		u8* it = parent->widget_buffer;

		foreach(i, parent->widget_count) {

			GuiWidget* widget = (GuiWidget*)it;

			gui_widget_draw(widget);
			it += sizeof(GuiWidget) + gui_widget_size(widget->type);
		}
	}

	imrend_pop_scissor(gui->cmd);
}

static GuiParent* allocate_parent()
{
	if (gui->parent_count >= PARENTS_MAX) {
		assert_title(FALSE, "Parent limit exceeded");
		return NULL;
	}
	return gui->parents + gui->parent_count++;
}

static void free_parents()
{
	memory_zero(gui->parents, sizeof(GuiParent) * gui->parent_count);
	gui->parent_count = 0;
}

//////////////////////////// BUFFER WRITES //////////////////////////////////

void gui_write_(const void* data, u32 size)
{
	buffer_write_back(&gui->buffer, data, size);
}

void gui_write_text(const char* text)
{
	text = string_validate(text);
	buffer_write_back(&gui->buffer, text, string_size(text) + 1);
}

u64 gui_write_widget(u32 type, u64 flags, u64 id)
{
	id = hash_combine(id, gui->current_id);

	GuiHeader header = GuiHeader_Widget;
	gui_write(header);
	gui_write(type);
	gui_write(flags);
	gui_write(id);

	return id;
}

//////////////////////////// BUFFER READS //////////////////////////////////

void gui_read_(u8** it_, void* data, u32 size)
{
	u8* it = *it_;
	memory_copy(data, it, size);
	it += size;
	*it_ = it;
}

void gui_read_text_(u8** it_, const char** text)
{
	u8* it = *it_;
	*text = (const char*)it;
	it += string_size(*text) + 1;
	*it_ = it;
}

const void* gui_read_buffer_(u8** it_, u32 size)
{
	u8* it = *it_;
	*it_ += size;
	return it;
}

//////////////////////////// FOCUS ////////////////////////////

void gui_free_focus()
{
	gui->focus.type = u32_max;
	gui->focus.id = 0;
	gui->focus.action = 0;
	gui->focus.parent_id = 0;
	gui->focus.widget = NULL;
	gui->focus.parent = NULL;
}

void gui_set_focus(GuiWidget* widget, u64 parent_id, u32 action)
{
	gui->focus.type = widget->type;
	gui->focus.id = widget->id;
	gui->focus.action = action;
	gui->focus.widget = widget;
	gui->focus.parent_id = parent_id;
	gui->focus.parent = NULL;
}

GuiFocus gui_get_focus()
{
	return gui->focus;
}

b8 gui_has_focus()
{
	return gui->focus.type != u32_max;
}

/////////////////////////// RUNTIME ////////////////////////////////

static void register_default_layouts();

b8 gui_initialize()
{
	gui = memory_allocate(sizeof(GUI));

	gui->buffer = buffer_init(1.7f);
	gui->id_stack = array_init(u64, 2.f);

	gui->focus.type = u32_max;
	gui->focus.id = 0;

	gui->widget_register_count = 1;
	gui->layout_register_count = 1;

	// register_default_widgets();
	register_default_layouts();
	
	return TRUE;
}

void gui_close()
{
	if (gui) {

		buffer_close(&gui->buffer);
		array_close(&gui->id_stack);

		memory_free(gui);
	}
}

void gui_begin(const char* layout, Font* default_font)
{
	buffer_reset(&gui->buffer);
	array_reset(&gui->id_stack);
	gui->current_id = 69;

	v2_u32 size = window_size();

	gui->default_font = default_font;
	gui->resolution = v2_set(size.x, size.y);
	gui->aspect = gui->resolution.x / gui->resolution.y;
	gui->pixel = v2_set(1.f / gui->resolution.x, 1.f / gui->resolution.y);
	gui->mouse_position = v2_add_scalar(input_mouse_position(), 0.5f);

	gui->parent_stack[0] = &gui->root;
	gui->parent_stack_count = 1;

	u32 layout_index = gui_find_layout_index(layout);
	if (layout_index == u32_max) {
		SV_LOG_ERROR("Invalid root layout name: %s\n", layout);
		layout_index = 0;
	}
	gui->root.layout.type = layout_index;

	gui->root.background.image = NULL;
	gui->root.background.texcoord = v4_zero();
	gui->root.background.color = color_rgba(0, 0, 0, 0);
}

void gui_end()
{
	// Reset state
	{
		gui->focus.widget = NULL;
		gui->focus.parent = NULL;
		
		memory_zero(gui->root.widget_buffer, gui->root.widget_buffer_size);
		gui->root.widget_buffer_size = 0;
		gui->root.widget_count = 0;
		gui->root.bounds = v4_set(0.5f, 0.5f, 1.f, 1.f);
		gui->root.widget_bounds = gui->root.bounds;
		gui_initialize_layout(&gui->root);

		gui->parent_stack[0] = &gui->root;
		gui->parent_stack_count = 1;

		free_parents();
	}
	
	// Read buffer
	{
		u8* it = gui->buffer.data;
		u8* end = it + gui->buffer.size;

		while (it < end) {
		
			GuiHeader header;
			gui_read(it, header);

			switch (header) {

			case GuiHeader_Widget:
			{			
				u32 type;
				
				gui_read(it, type);

				u32 widget_size = gui_widget_size(type);

				GuiParent* parent = gui_current_parent();

				if (parent->widget_buffer_size + widget_size + sizeof(GuiWidget) >= _GUI_PARENT_WIDGET_BUFFER_SIZE) {
					SV_LOG_ERROR("The parent buffer has a limit of %u bytes\n", _GUI_PARENT_WIDGET_BUFFER_SIZE);
					return;
				}

				GuiWidget* widget = (GuiWidget*)(parent->widget_buffer + parent->widget_buffer_size);

				gui_read(it, widget->flags);
				gui_read(it, widget->id);
				widget->type = type;

				parent->widget_buffer_size += widget_size + sizeof(GuiWidget);
				parent->widget_count++;
				
				it = gui_widget_read(widget, it);

				// Set bounds
				widget->bounds = gui_compute_bounds(parent);
			}
			break;

			case GuiHeader_BeginParent:
			{
				GuiParent* current = gui_current_parent();

				GuiParent* parent = allocate_parent();
				parent->parent = current;
				gui_read(it, parent->id);
				gui_read(it, parent->layout.type);
				parent->bounds = gui_compute_bounds(current);

				if (current) {
					v4 pb = parent->bounds;
					v4 b = current->bounds;
		
					pb.x = (pb.x * b.z) + b.x - (b.z * 0.5f);
					pb.y = (pb.y * b.w) + b.y - (b.w * 0.5f);
					pb.z *= b.z;
					pb.w *= b.w;

					parent->bounds = pb;
				}
				
				parent->widget_bounds = parent->bounds;

				gui_push_parent(parent);
				gui_initialize_layout(parent);
			}
			break;

			case GuiHeader_EndParent:
			{
				gui_pop_parent();
			}
			break;

			case GuiHeader_SetBackground:
			{
				GPUImage* image;
				v4 texcoord;
				Color color;
				gui_read(it, image);
				gui_read(it, texcoord);
				gui_read(it, color);

				GuiParent* parent = gui_current_parent();
				if (parent) {
					parent->background.image = image;
					parent->background.texcoord = texcoord;
					parent->background.color = color;
				}
			}
			break;

			case GuiHeader_LayoutPush:
			case GuiHeader_LayoutSet:
			{
				u16 property;
				u16 size;
				gui_read(it, property);

				if (property != 0) {

					gui_read(it, size);

					GuiParent* parent = gui_current_parent();
					if (parent) {

						GuiLayout* layout = &parent->layout;

						if (header == GuiHeader_LayoutPush) {

							u8* stack = layout->stack + layout->stack_size;
							b8 res = gui_layout_property_read(parent, property, it, size, stack);

							if (!res) {
								size = 0;
							}

							stack += size;

							SV_BUFFER_WRITE(stack, property);
							SV_BUFFER_WRITE(stack, size);

							layout->stack_size = stack - layout->stack;
						}
						else {

							gui_layout_property_read(parent, property, it, size, NULL);
						}
					}

					it += size;
				}
			}
			break;

			case GuiHeader_LayoutPop:
			{
				u32 count;
				gui_read(it, count);

				GuiParent* parent = gui_current_parent();
				if (parent) {

					GuiLayout* layout = &parent->layout;

					u8* stack = layout->stack + layout->stack_size;

					foreach(i, count) {

						u16 size;
						u16 property;

						if (stack - layout->stack >= sizeof(u16)) {
							stack -= sizeof(u16);
							memory_copy(&size, stack, sizeof(u16));
						}
						else break;

						if (stack - layout->stack >= sizeof(u16)) {
							stack -= sizeof(u16);
							memory_copy(&property, stack, sizeof(u16));
						}
						else break;

						if_assert(size && stack - layout->stack >= size) {

							stack -= size;

							gui_layout_property_read(parent, property, stack, size, NULL);
						}
						else break;
					}

					layout->stack_size = stack - layout->stack;
				}
			}
			break;

			case GuiHeader_WidgetPush:
			{
				u16 type;
				u16 property;
				u16 size;
				gui_read(it, type);
				gui_read(it, property);

				if (property != 0) {

					gui_read(it, size);

					u8* stack = gui->widget_registers[type].stack;
					u32 stack_size = gui->widget_registers[type].stack_size;

					u8* s = stack + stack_size;

					b8 res = gui_widget_property_read(type, property, it, size, s);

					if (!res) {
						size = 0;
					}

					s += size;

					SV_BUFFER_WRITE(s, property);
					SV_BUFFER_WRITE(s, size);

					gui->widget_registers[type].stack_size = s - stack;

					it += size;
				}
			}
			break;

			case GuiHeader_WidgetPop:
			{
				u16 type;
				u32 count;
				gui_read(it, type);
				gui_read(it, count);

				u8* stack = gui->widget_registers[type].stack + gui->widget_registers[type].stack_size;

				foreach(i, count) {

					u16 size;
					u16 property;

					if (stack - gui->widget_registers[type].stack >= sizeof(u16)) {
						stack -= sizeof(u16);
						memory_copy(&size, stack, sizeof(u16));
					}
					else break;

					if (stack - gui->widget_registers[type].stack >= sizeof(u16)) {
						stack -= sizeof(u16);
						memory_copy(&property, stack, sizeof(u16));
					}
					else break;

					if_assert(size && stack - gui->widget_registers[type].stack >= size) {

						stack -= size;

						gui_widget_property_read(type, property, stack, size, NULL);
					}
					else break;
				}

				gui->widget_registers[type].stack_size = stack - gui->widget_registers[type].stack;
			}
			break;

			default:
			{
				assert_title(FALSE, "GUI buffer corrupted");
			}
			break;
			
			}
		}
	}

	// Adjust widget bounds
	{
		foreach(i, gui->parent_count) {

			GuiParent* parent = gui->parents + i;
			
			adjust_widget_bounds(parent);
		}

		adjust_widget_bounds(&gui->root);
	}

	// Update focus
	{
		if (gui->focus.type != u32_max) {

			gui->focus.parent = gui_find_parent(gui->focus.parent_id);

			if (gui->focus.parent) {

				gui->focus.widget = gui_find_widget(gui->focus.type, gui->focus.id, gui->focus.parent);
				if (gui->focus.widget != NULL)
					gui_widget_update(gui->focus.parent, gui->focus.widget, TRUE);
				else gui_free_focus();
			}
			else gui_free_focus();
		}
	}
	
	// Update widget
	{
		foreach(i, gui->parent_count) {

			GuiParent* parent = gui->parents + i;
			update_parent(parent);
		}

		update_parent(&gui->root);
	}
}

void gui_draw(GPUImage* image, CommandList cmd)
{
	gui->cmd = cmd;
	gui->image = image;

	imrend_begin_batch(image, NULL, cmd);
	imrend_camera(ImRendCamera_Normal, cmd);

	draw_parent(&gui->root);

	imrend_flush(cmd);
}

///////////////////////// IDS ///////////////////////////

void gui_push_id(u64 id)
{
	array_push(&gui->id_stack, id);

	gui->current_id = 69;

	foreach(i, gui->id_stack.size) {

		gui->current_id = hash_combine(gui->current_id, *(u64*)array_get(&gui->id_stack, i));
	}
}

void gui_pop_id(u32 count)
{
	if (gui->id_stack.size >= count)
		array_pop_count(&gui->id_stack, count);
	else {
		assert_title(FALSE, "Can't pop gui id");
	}

	gui->current_id = 69;

	foreach(i, gui->id_stack.size) {

		gui->current_id = hash_combine(gui->current_id, *(u64*)array_get(&gui->id_stack, i));
	}
}

void gui_begin_parent(const char* layout)
{
	GuiHeader header = GuiHeader_BeginParent;
	gui_write(header);
	u64 id = hash_combine(0x83487, gui->current_id);
	gui_write(id);

	u32 layout_index = gui_find_layout_index(layout);
	if (layout_index == u32_max) {
		SV_LOG_ERROR("Invalid layout '%s'\n", layout);
		layout_index = 0;
	}

	gui_write(layout_index);

	GuiParent* parent = gui_find_parent(id);
	gui_push_parent(parent);

	gui_push_id(id);
}

void gui_end_parent()
{
	GuiHeader header = GuiHeader_EndParent;
	gui_write(header);

	gui_pop_parent();
	gui_pop_id(1);
}

void gui_set_background(GPUImage* image, v4 texcoord, Color color)
{
	GuiHeader header = GuiHeader_SetBackground;
	gui_write(header);
	gui_write(image);
	gui_write(texcoord);
	gui_write(color);
}

////////////////////////////// REGISTERS ///////////////////////////////

u32 gui_register_widget(const GuiRegisterWidgetDesc* desc)
{
	u32 i = gui->widget_register_count++;

	string_copy(gui->widget_registers[i].name, desc->name, NAME_SIZE);
	gui->widget_registers[i].read_fn = desc->read_fn;
	gui->widget_registers[i].update_fn = desc->update_fn;
	gui->widget_registers[i].draw_fn = desc->draw_fn;
	gui->widget_registers[i].property_id_fn = desc->property_id_fn;
	gui->widget_registers[i].property_read_fn = desc->property_read_fn;
	gui->widget_registers[i].size = desc->size;

	return i;
}

u32 gui_register_layout(const GuiRegisterLayoutDesc* desc)
{
	u32 i = gui->layout_register_count++;

	string_copy(gui->layout_registers[i].name, desc->name, NAME_SIZE);
	gui->layout_registers[i].initialize_fn = desc->initialize_fn;
	gui->layout_registers[i].compute_bounds_fn = desc->compute_bounds_fn;
	gui->layout_registers[i].property_read_fn = desc->property_read_fn;
	gui->layout_registers[i].property_id_fn = desc->property_id_fn;

	return i;
}

//////////////////////////////// WIDGET UTILS //////////////////////////////////

Font* gui_font()
{
	return gui->default_font;
}

v2 gui_resolution() 
{
	return gui->resolution;
}

v2 gui_pixel_size()
{
	return gui->pixel;
}

v2 gui_mouse_position()
{
	return gui->mouse_position;
}

f32 gui_aspect()
{
	return gui->aspect;
}

void gui_draw_bounds(v4 bounds, Color color)
{
	imrend_draw_quad(v3_set(bounds.x, bounds.y, 0.f), v2_set(bounds.z, bounds.w), color, gui->cmd);
}

b8 gui_mouse_in_bounds(v4 bounds)
{
	return(fabs(gui->mouse_position.x - bounds.x) <= bounds.z * 0.5f && fabs(gui->mouse_position.y - bounds.y) <= bounds.w * 0.5f);
}

void gui_draw_sprite(v4 bounds, Color color, GPUImage* image, v4 tc)
{
	imrend_draw_sprite(v3_set(bounds.x, bounds.y, 0.f), v2_set(bounds.z, bounds.w), color, image, GPUImageLayout_ShaderResource, tc, gui->cmd);
}

void gui_draw_text(const char* text, v4 bounds, TextAlignment alignment)
{
	TextContext ctx;
	SV_ZERO(ctx);

	ImRendDrawTextDesc desc;
	desc.flags = DrawTextFlag_LineCentred;
	desc.text = text;
	desc.font = gui->default_font;
	desc.font_size = 1.f;
	desc.alignment = alignment;
	desc.aspect = bounds.z / bounds.w * window_aspect();
	desc.transform_matrix = m4_mul(m4_scale(bounds.z, bounds.w, 1.f), m4_translate(bounds.x - bounds.z * 0.5f, bounds.y - bounds.w * 0.5f, 0.f));
	desc.max_lines = 1;
	desc.context = &ctx;
	desc.text_default_color = color_white();
	desc.text_selected_color = color_white();
	desc.cursor_color = color_white();

	imrend_draw_text(&desc, gui->cmd);
}

GuiWidget* gui_find_widget(u32 type, u64 id, GuiParent* parent)
{
	// TODO: Optimize?

	if (parent == NULL)
		return NULL;
	
	u8* it = parent->widget_buffer;
	foreach(i, parent->widget_count) {

		GuiWidget* widget = (GuiWidget*)it;
		if (widget->type == type && widget->id == id) {
			return widget;
		}

		it += sizeof(GuiWidget) + gui_widget_size(widget->type);
	}

	return NULL;
}

GuiParent* gui_find_parent(u64 parent_id)
{
	if (parent_id == 0) return &gui->root;

	foreach(i, gui->parent_count) {

		if (gui->parents[i].id == parent_id)
			return gui->parents + i;
	}

	return NULL;
}

GuiParent* gui_current_parent()
{
	if (gui->parent_stack_count == 0)
		return NULL;

	return gui->parent_stack[gui->parent_stack_count - 1];
}

f32 gui_compute_coord(GuiCoord coord, b8 vertical, f32 dimension, f32 parent_dimension)
{
	f32 value = 0.f;

	f32 pixel = vertical ? gui->pixel.y : gui->pixel.x;

	switch (coord.unit)
	{

	case GuiUnit_Relative:
		value = coord.value;
		break;

	case GuiUnit_Pixel:
		value = coord.value / parent_dimension * pixel;
		break;

	}

	switch (coord.align)
	{

	case GuiCoordAlign_Left:
	case GuiCoordAlign_Bottom:
	case GuiCoordAlign_InverseRight:
	case GuiCoordAlign_InverseTop:
		value += dimension * 0.5f;
		break;

	case GuiCoordAlign_Right:
	case GuiCoordAlign_Top:
	case GuiCoordAlign_InverseLeft:
	case GuiCoordAlign_InverseBottom:
	
		value -= dimension * 0.5f;
		break;

	}

	if (coord.align == GuiCoordAlign_InverseLeft || coord.align == GuiCoordAlign_InverseRight || coord.align == GuiCoordAlign_InverseBottom || coord.align == GuiCoordAlign_InverseTop || coord.align == GuiCoordAlign_InverseCenter) {
		value = 1.f - value;
	}

	return value;
}

f32 gui_compute_dimension(GuiDimension dimension, b8 vertical, f32 parent_dimension)
{
	f32 value = 0.f;

	f32 pixel = vertical ? gui->pixel.y : gui->pixel.x;

	switch (dimension.unit)
	{

	case GuiUnit_Relative:
		value = dimension.value;
		break;

	case GuiUnit_Pixel:
		value = dimension.value / parent_dimension * pixel;
		break;

	}

	return value;
}

///////////////////////////////// LAYOUT STACK ///////////////////////////////

void gui_layout_push_ex(const char* name, const void* data, u16 size)
{
	GuiParent* parent = gui_current_parent();

	if (parent != NULL) {

		u16 property_id = gui_layout_property_id(parent, name);

		GuiHeader header = GuiHeader_LayoutPush;
		gui_write(header);
		gui_write(property_id);

		if (property_id != 0) {

			gui_write(size);
			gui_write_(data, size);
		}
	}
}

void gui_layout_set_ex(const char* name, const void* data, u16 size)
{
	GuiParent* parent = gui_current_parent();

	if (parent != NULL) {

		u16 property_id = gui_layout_property_id(parent, name);

		GuiHeader header = GuiHeader_LayoutSet;
		gui_write(header);
		gui_write(property_id);

		if (property_id != 0) {

			gui_write(size);
			gui_write_(data, size);
		}
	}
}

void gui_layout_pop(u32 count)
{
	GuiParent* parent = gui_current_parent();

	if (parent != NULL) {

		GuiHeader header = GuiHeader_LayoutPop;
		gui_write(header);
		gui_write(count);
	}
}

void gui_widget_push_ex(u32 widget_id, const char* name, const void* data, u16 size)
{
	u16 type_ = widget_id;
	u16 property_id = gui_widget_property_id(widget_id, name);

	GuiHeader header = GuiHeader_WidgetPush;
	gui_write(header);
	gui_write(type_);
	gui_write(property_id);

	if (property_id != 0) {

		gui_write(size);
		gui_write_(data, size);
	}
}

void gui_widget_pop(u32 widget_id, u32 count)
{
	u16 type_ = widget_id;
	GuiHeader header = GuiHeader_WidgetPop;
	gui_write(header);
	gui_write(type_);
	gui_write(count);
}

////////////////////////////////////////////////// FREE LAYOUT ///////////////////////////////////////////////

typedef struct {
	GuiFreeLayoutData data;
} GuiFreeLayoutInternal;

static void free_layout_initialize(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiFreeLayoutInternal* d = (GuiFreeLayoutInternal*)layout->data;
	d->data.x = (GuiCoord) { 0.5f, GuiUnit_Relative, GuiCoordAlign_Center };
	d->data.y = (GuiCoord) { 0.5f, GuiUnit_Relative, GuiCoordAlign_Center };
	d->data.width = (GuiDimension) { 1.f, GuiUnit_Relative };
	d->data.height = (GuiDimension) { 1.f, GuiUnit_Relative };
}

static v4 free_layout_compute_bounds(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiFreeLayoutInternal* d = (GuiFreeLayoutInternal*)layout->data;

	f32 width = gui_compute_dimension(d->data.width, FALSE, parent->widget_bounds.z);
	f32 height = gui_compute_dimension(d->data.height, TRUE, parent->widget_bounds.w);
	f32 x = gui_compute_coord(d->data.x, FALSE, width, parent->widget_bounds.z);
	f32 y = gui_compute_coord(d->data.y, TRUE, height, parent->widget_bounds.w);
	
	return v4_set(x, y, width, height);
}

static u16 free_layout_property_id(const char* name)
{
	if (string_equals(name, "x")) {
		return 1;
	}
	if (string_equals(name, "y")) {
		return 2;
	}
	if (string_equals(name, "width")) {
		return 3;
	}
	if (string_equals(name, "height")) {
		return 4;
	}

	return 0;
}

static b8 free_layout_property_read(GuiParent* parent, u32 property, u8* it, u32 size, u8* pop_data)
{
	GuiLayout* layout = &parent->layout;
	GuiFreeLayoutInternal* d = (GuiFreeLayoutInternal*)layout->data;

	u8* dst = NULL;

	switch (property)
	{
	case 1:
		if_assert(size == sizeof(d->data.x))
			dst = (u8*)&d->data.x;
		break;

	case 2:
		if_assert(size == sizeof(d->data.y))
			dst = (u8*)&d->data.y;
		break;

	case 3:
		if_assert(size == sizeof(d->data.width))
			dst = (u8*)&d->data.width;
		break;

	case 4:
		if_assert(size == sizeof(d->data.height))
			dst = (u8*)&d->data.height;
		break;
	}

	if (dst != NULL) {

		if (pop_data)
			memory_copy(pop_data, dst, size);
		gui_read_(&it, dst, size);

		return TRUE;
	}

	return FALSE;
}

static void register_default_layouts()
{
	GuiRegisterLayoutDesc desc;

	desc.name = "free";
	desc.initialize_fn = free_layout_initialize;
	desc.compute_bounds_fn = free_layout_compute_bounds;
	desc.property_read_fn = free_layout_property_read;
	desc.property_id_fn = free_layout_property_id;
	gui->register_ids.layout_free = gui_register_layout(&desc);
}
