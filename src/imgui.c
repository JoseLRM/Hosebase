#include "Hosebase/imgui.h"

#define PARENTS_MAX 100

typedef enum {
	GuiHeader_Widget,
	GuiHeader_LayoutUpdate,
	GuiHeader_BeginParent,
	GuiHeader_EndParent,
	GuiHeader_SetBackground,
} GuiHeader;

typedef struct {
	char name[NAME_SIZE];
	u8*(*read_fn)(GuiWidget* widget, u8* it);
	u8*(*update_fn)(GuiParent* parent, GuiWidget* widget, b8 has_focus);
	u8*(*draw_fn)(GuiWidget* widget);
	u32 size;
} GuiWidgetRegister;

typedef struct {
	void(*initialize_fn)(GuiParent* parent);
	v4(*compute_bounds_fn)(GuiParent* parent);
	u8*(*update_fn)(GuiParent* parent, u8* it);
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
		u32 widget_button;

		u32 layout_stack;
		u32 layout_free;
		u32 layout_grid;
	} register_ids;
	
} GUI;

static GUI* gui;

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

static void gui_initialize_layout(GuiParent* parent)
{
	memory_zero(&parent->layout.data, GUI_LAYOUT_DATA_SIZE);
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

static u8* gui_layout_update(GuiParent* parent, u8* it)
{
	u32 type = parent->layout.type;

	if (type >= gui->layout_register_count) return it;
	return gui->layout_registers[type].update_fn(parent, it);
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

	gui_draw_sprite(b, parent->background.color, parent->background.image, parent->background.texcoord);

	u8* it = parent->widget_buffer;

	foreach(i, parent->widget_count) {

		GuiWidget* widget = (GuiWidget*)it;

		gui_widget_draw(widget);
		it += sizeof(GuiWidget) + gui_widget_size(widget->type);
	}

	imrend_pop_scissor(gui->cmd);
}

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

b8 imgui_write_layout_update(u32 type)
{
	GuiParent* parent = gui_current_parent();
	if (parent == NULL)
		return FALSE;

	if (parent->layout.type == type) {

		GuiHeader header = GuiHeader_LayoutUpdate;
		gui_write(header);

		return TRUE;
	}
	
	return FALSE;
}

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

Font* gui_font()
{
	return gui->default_font;
}

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

static void register_default_widgets();
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

	register_default_widgets();
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

				if (parent->widget_buffer_size + widget_size + sizeof(GuiWidget) >= GUI_PARENT_WIDGET_BUFFER_SIZE) {
					SV_LOG_ERROR("The parent buffer has a limit of %u bytes\n", GUI_PARENT_WIDGET_BUFFER_SIZE);
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

			case GuiHeader_LayoutUpdate:
			{
				GuiParent* parent = gui_current_parent();

				it = gui_layout_update(parent, it);
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

	foreach(i, gui->parent_count) {

		GuiParent* parent = gui->parents + i;
		draw_parent(parent);
	}

	draw_parent(&gui->root);

	imrend_flush(cmd);
}

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

u32 gui_register_widget(const GuiRegisterWidgetDesc* desc)
{
	u32 i = gui->widget_register_count++;

	string_copy(gui->widget_registers[i].name, desc->name, NAME_SIZE);
	gui->widget_registers[i].read_fn = desc->read_fn;;
	gui->widget_registers[i].update_fn = desc->update_fn;
	gui->widget_registers[i].draw_fn = desc->draw_fn;
	gui->widget_registers[i].size = desc->size;

	return i;
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

//////////////////////////////// WIDGET UTILS //////////////////////////////////

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

///////////////////////////////// STACK LAYOUT ///////////////////////////////

typedef struct {
	GuiStackLayoutData data;
	f32 offset;
} GuiStackLayoutInternal;

GuiStackLayoutData gui_stack_layout_get_data()
{
	GuiLayout* layout = &gui->root.layout;
	GuiStackLayoutInternal* d = (GuiStackLayoutInternal*)layout->data;
	return d->data;
}

void gui_stack_layout_update(GuiStackLayoutData data)
{
	u32 type = gui->register_ids.layout_stack;
	if (imgui_write_layout_update(type)) {

		u8 update = 0;
		gui_write(update);
		gui_write_(&data, sizeof(data));
	}
}

void gui_stack_layout_update_size(f32 width, GuiUnit width_unit, f32 height, GuiUnit height_unit)
{
	u32 type = gui->register_ids.layout_stack;
	if (imgui_write_layout_update(type)) {

		u8 update = 1;

		GuiDimension w, h;
		w.value = width;
		w.unit = width_unit;
		h.value = height;
		h.unit = height_unit;

		gui_write(update);
		gui_write(w);
		gui_write(h);
	}
}

static void stack_layout_initialize(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiStackLayoutInternal* d = (GuiStackLayoutInternal*)layout->data;
	d->data.width = (GuiDimension){ 0.9f, GuiUnit_Relative };
	d->data.height = (GuiDimension){ 50.f, GuiUnit_Pixel };
	d->data.margin = (GuiDimension){ 0.05f, GuiUnit_Relative };
	d->data.padding = (GuiDimension){ 10.f, GuiUnit_Pixel };
	d->offset = gui_compute_dimension(d->data.margin, TRUE, parent->widget_bounds.w);
}

static v4 stack_layout_compute_bounds(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiStackLayoutInternal* d = (GuiStackLayoutInternal*)layout->data;

	f32 width = gui_compute_dimension(d->data.width, FALSE, parent->widget_bounds.z);
	f32 height = gui_compute_dimension(d->data.height, TRUE, parent->widget_bounds.w);
	f32 margin = gui_compute_dimension(d->data.margin, TRUE, parent->widget_bounds.w);
	f32 padding = gui_compute_dimension(d->data.padding, TRUE, parent->widget_bounds.w);

	f32 y = 1.f - d->offset - height * 0.5f;
	d->offset += height + padding;
	
	return v4_set(0.5f, y, width, height);
}

static u8* stack_layout_update(GuiParent* parent, u8* it)
{
	GuiLayout* layout = &parent->layout;
	GuiStackLayoutInternal* d = (GuiStackLayoutInternal*)layout->data;
	
	u8 update;
	gui_read(it, update);

	switch (update)
	{
	case 0:
		gui_read(it, d->data);
		break;

	case 1:
	{
		gui_read(it, d->data.width);
		gui_read(it, d->data.height);
	}
	break;
	
	}
	
	return it;
}

////////////////////////////////////////////////// FREE LAYOUT ///////////////////////////////////////////////

typedef struct {
	GuiFreeLayoutData data;
} GuiFreeLayoutInternal;

GuiFreeLayoutData gui_free_layout_get_data()
{
	GuiLayout* layout = &gui->root.layout;
	GuiFreeLayoutInternal* d = (GuiFreeLayoutInternal*)layout->data;
	return d->data;
}

void gui_free_layout_update(GuiFreeLayoutData data)
{
	u32 type = gui->register_ids.layout_free;
	if (imgui_write_layout_update(type)) {

		u8 update = 0;
		gui_write(update);
		gui_write_(&data, sizeof(data));
	}
}

void gui_free_layout_update_x(f32 value, GuiUnit unit, GuiCoordAlign align)
{
	u32 type = gui->register_ids.layout_free;
	if (imgui_write_layout_update(type)) {

		u8 update = 1;

		GuiCoord coord;
		coord.value = value;
		coord.unit = unit;
		coord.align = align;

		gui_write(update);
		gui_write(coord);
	}
}

void gui_free_layout_update_y(f32 value, GuiUnit unit, GuiCoordAlign align)
{
	u32 type = gui->register_ids.layout_free;
	if (imgui_write_layout_update(type)) {

		u8 update = 2;

		GuiCoord coord;
		coord.value = value;
		coord.unit = unit;
		coord.align = align;

		gui_write(update);
		gui_write(coord);
	}
}

void gui_free_layout_update_width(f32 value, GuiUnit unit)
{
	u32 type = gui->register_ids.layout_free;
	if (imgui_write_layout_update(type)) {

		u8 update = 3;

		GuiDimension dim;
		dim.value = value;
		dim.unit = unit;

		gui_write(update);
		gui_write(dim);
	}
}

void gui_free_layout_update_height(f32 value, GuiUnit unit)
{
	u32 type = gui->register_ids.layout_free;
	if (imgui_write_layout_update(type)) {

		u8 update = 4;

		GuiDimension dim;
		dim.value = value;
		dim.unit = unit;

		gui_write(update);
		gui_write(dim);
	}
}

static void free_layout_initialize(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiFreeLayoutInternal* d = (GuiFreeLayoutInternal*)layout->data;
	d->data.x = (GuiCoord) { 0.5f, GuiUnit_Relative, GuiCoordAlign_Center };
	d->data.y = (GuiCoord) { 0.5f, GuiUnit_Relative, GuiCoordAlign_Center };
	d->data.width = (GuiDimension) { 0.5f, GuiUnit_Relative };
	d->data.height = (GuiDimension) { 1.f, GuiUnit_Aspect };
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

static u8* free_layout_update(GuiParent* parent, u8* it)
{
	GuiLayout* layout = &parent->layout;
	GuiFreeLayoutInternal* d = (GuiFreeLayoutInternal*)layout->data;
	
	u8 update;
	gui_read(it, update);

	switch (update)
	{
	case 0:
		gui_read(it, d->data);
		break;

	case 1:
		gui_read(it, d->data.x);
		break;

	case 2:
		gui_read(it, d->data.y);
		break;

	case 3:
		gui_read(it, d->data.width);
		break;

	case 4:
		gui_read(it, d->data.height);
		break;
	
	}
	
	return it;
}

/////////////////////////// GRID LAYOUT //////////////////////////////

typedef struct {
	GuiGridLayoutData data;
	u32 count;
} GuiGridLayoutInternal;

GuiGridLayoutData gui_grid_layout_get_data()
{
	GuiLayout* layout = &gui->root.layout;
	GuiGridLayoutInternal* d = (GuiGridLayoutInternal*)layout->data;
	return d->data;
}

void gui_grid_layout_update(GuiGridLayoutData data)
{
	u32 type = gui->register_ids.layout_grid;
	if (imgui_write_layout_update(type)) {

		u8 update = 0;
		gui_write(update);
		gui_write_(&data, sizeof(data));
	}
}

void gui_grid_layout_update_height(f32 height, GuiUnit height_unit)
{
	u32 type = gui->register_ids.layout_grid;
	if (imgui_write_layout_update(type)) {

		u8 update = 1;

		GuiDimension h;
		h = (GuiDimension){ height, height_unit };

		gui_write(update);
		gui_write(h);
	}
}

void gui_grid_layout_update_columns(u32 columns)
{
	u32 type = gui->register_ids.layout_grid;
	if (imgui_write_layout_update(type)) {

		u8 update = 2;

		gui_write(update);
		gui_write(columns);
	}
}

static void grid_layout_initialize(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiGridLayoutInternal* d = (GuiGridLayoutInternal*)layout->data;
	d->data.height = (GuiDimension){ 1.f, GuiUnit_Aspect };
	d->data.horizontal_margin = (GuiDimension){ 10.f, GuiUnit_Pixel };
	d->data.vertical_margin = (GuiDimension){ 10.f, GuiUnit_Pixel };
	d->data.horizontal_padding = (GuiDimension){ 5.f, GuiUnit_Pixel };
	d->data.vertical_padding = (GuiDimension){ 5.f, GuiUnit_Pixel };
	d->data.columns = 5;
	d->count = 0;
}

static v4 grid_layout_compute_bounds(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiGridLayoutInternal* d = (GuiGridLayoutInternal*)layout->data;

	f32 row = (f32)(d->count / d->data.columns);
	f32 column = (f32)(d->count % d->data.columns);

	f32 h_margin = gui_compute_dimension(d->data.horizontal_margin, FALSE, parent->widget_bounds.z);
	f32 v_margin = gui_compute_dimension(d->data.vertical_margin, TRUE, parent->widget_bounds.w);
	f32 h_padding = gui_compute_dimension(d->data.horizontal_padding, FALSE, parent->widget_bounds.z);
	f32 v_padding = gui_compute_dimension(d->data.vertical_padding, TRUE, parent->widget_bounds.w);

	f32 width = (1.f - h_margin * 2.f - (f32)d->data.columns * h_padding) / (f32)d->data.columns;
	f32 height = gui_compute_dimension(d->data.height, TRUE, parent->widget_bounds.w);

	f32 x = h_margin + column * (width + h_padding) + width * 0.5f;
	f32 y = 1.f - (v_margin + row * (height + v_padding)) - height * 0.5f;

	d->count++;

	return v4_set(x, y, width, height);
}

static u8* grid_layout_update(GuiParent* parent, u8* it)
{
	GuiLayout* layout = &parent->layout;
	GuiGridLayoutInternal* d = (GuiGridLayoutInternal*)layout->data;

	u8 update;
	gui_read(it, update);

	switch (update)
	{
	case 0:
		gui_read(it, d->data);
		break;

	case 1:
		gui_read(it, d->data.height);
		break;

	case 2:
		gui_read(it, d->data.columns);
		break;

	}

	return it;
}

static void register_default_layouts()
{
	gui->layout_registers[1].initialize_fn = stack_layout_initialize;
	gui->layout_registers[1].compute_bounds_fn = stack_layout_compute_bounds;
	gui->layout_registers[1].update_fn = stack_layout_update;
	string_copy(gui->layout_registers[1].name, "stack", NAME_SIZE);
	gui->register_ids.layout_stack = 1;

	gui->layout_registers[2].initialize_fn = free_layout_initialize;
	gui->layout_registers[2].compute_bounds_fn = free_layout_compute_bounds;
	gui->layout_registers[2].update_fn = free_layout_update;
	string_copy(gui->layout_registers[2].name, "free", NAME_SIZE);
	gui->register_ids.layout_free = 2;

	gui->layout_registers[3].initialize_fn = grid_layout_initialize;
	gui->layout_registers[3].compute_bounds_fn = grid_layout_compute_bounds;
	gui->layout_registers[3].update_fn = grid_layout_update;
	string_copy(gui->layout_registers[3].name, "grid", NAME_SIZE);
	gui->register_ids.layout_grid = 3;

	gui->layout_register_count = 4;
}
