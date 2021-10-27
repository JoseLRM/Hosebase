#include "Hosebase/imgui.h"

#define PARENT_WIDGET_BUFFER_SIZE (500 * 30)
#define LAYOUT_DATA_SIZE 30
#define PARENTS_MAX 100

typedef struct GuiParent GuiParent;
typedef struct GuiWidget GuiWidget;

typedef struct {
	u32 type;
	u8 data[LAYOUT_DATA_SIZE];
} GuiLayout;

struct GuiParent {
	u64 id;
	v4 bounds;
	v4 widget_bounds;
	u8 widget_buffer[PARENT_WIDGET_BUFFER_SIZE];
	u32 widget_buffer_size;
	u32 widget_count;
	GuiParent* parent;
	GuiLayout layout;
};

typedef enum {
	GuiHeader_Widget,
	GuiHeader_LayoutUpdate,
	GuiHeader_BeginParent,
	GuiHeader_EndParent
} GuiHeader;

typedef struct {
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

	struct {
		u64 parent_id;
		GuiParent* parent;
		u64 id;
		u32 type;
		GuiWidget* widget;
	} focus;

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
	} register_ids;
	
} GUI;

static GUI* gui;

struct GuiWidget {
	u64 id;
	u64 flags;
	u32 type;
	v4 bounds;
};

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
	gui->widget_registers[type].update_fn(parent, widget, has_focus);
}

static void gui_widget_draw(GuiWidget* widget)
{
	u32 type = widget->type;
	if (type >= gui->widget_register_count) return;
	gui->widget_registers[type].draw_fn(widget);
}

static void gui_initialize_layout(GuiParent* parent)
{
	memory_zero(&parent->layout, sizeof(GuiLayout));
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

static GuiWidget* gui_find_widget(u32 type, u64 id, GuiParent* parent)
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

static GuiParent* gui_find_parent(u64 parent_id)
{
	if (parent_id == 0) return &gui->root;

	foreach(i, gui->parent_count) {

		if (gui->parents[i].id == parent_id)
			return gui->parents + i;
	}

	return NULL;
}

static GuiParent* gui_current_parent()
{
	if (gui->parent_stack_count == 0)
		return NULL;

	return gui->parent_stack[gui->parent_stack_count - 1];
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
	u8* it = parent->widget_buffer;
	v4 pb = parent->widget_bounds;

	f32 inv_height = 1.f / gui->resolution.y;

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

	u8* it = parent->widget_buffer;

	foreach(i, parent->widget_count) {

		GuiWidget* widget = (GuiWidget*)it;

		gui_widget_draw(widget);
		it += sizeof(GuiWidget) + gui_widget_size(widget->type);
	}

	imrend_pop_scissor(gui->cmd);
}

static void gui_write_(const void* data, u32 size)
{
	buffer_write_back(&gui->buffer, data, size);
}

#define gui_write(data) gui_write_(&data, sizeof(data))

static void gui_write_text(const char* text)
{
	text = string_validate(text);
	buffer_write_back(&gui->buffer, text, string_size(text) + 1);
}

static u64 gui_write_widget(u32 type, u64 flags, u64 id)
{
	id = hash_combine(id, gui->current_id);

	GuiHeader header = GuiHeader_Widget;
	gui_write(header);
	gui_write(type);
	gui_write(flags);
	gui_write(id);

	return id;
}

static void imgui_write_layout_update(u32 type)
{
	GuiParent* parent = &gui->root;
	if (parent->layout.type == type) {

		GuiHeader header = GuiHeader_LayoutUpdate;
		gui_write(header);
	}
	else {
		
		assert_title(FALSE, "Invalid layout update");
	}
}

static void gui_read_(u8** it_, void* data, u32 size)
{
	u8* it = *it_;
	memory_copy(data, it, size);
	it += size;
	*it_ = it;
}

static void gui_read_text_(u8** it_, const char** text)
{
	u8* it = *it_;
	*text = (const char*)it;
	it += string_size(*text) + 1;
	*it_ = it;
}

#define gui_read(it, data) gui_read_(&it, &data, sizeof(data))
#define gui_read_text(it, text) gui_read_text_(&it, &text)

static void gui_free_focus()
{
	gui->focus.type = u32_max;
	gui->focus.id = 0;
	gui->focus.parent_id = 0;
	gui->focus.widget = NULL;
}

static void gui_set_focus(GuiWidget* widget, u64 parent_id)
{
	gui->focus.type = widget->type;
	gui->focus.id = widget->id;
	gui->focus.widget = widget;
	gui->focus.parent_id = parent_id;
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

				if (parent->widget_buffer_size + widget_size + sizeof(GuiWidget) >= PARENT_WIDGET_BUFFER_SIZE) {
					SV_LOG_ERROR("The parent buffer has a limit of %u bytes\n", PARENT_WIDGET_BUFFER_SIZE);
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
				gui_widget_update(gui->focus.parent, gui->focus.widget, TRUE);
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

	imrend_begin_batch(image, cmd);
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

//////////////////////////////// WIDGET UTILS //////////////////////////////////

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
	imrend_draw_sprite(v3_set(bounds.x, bounds.y, 0.f), v2_set(bounds.z, bounds.w), color, image, GPUImageLayout_RenderTarget, tc, gui->cmd);
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
	desc.transform_matrix = mat4_multiply(mat4_translate(bounds.x - bounds.z * 0.5f, bounds.y - bounds.w * 0.5f, 0.f), mat4_scale(bounds.z, bounds.w, 1.f));
	desc.max_lines = 1;
	desc.context = &ctx;
	desc.text_default_color = color_white();
	desc.text_selected_color = color_white();
	desc.cursor_color = color_white();

	imrend_draw_text(&desc, gui->cmd);
}

///////////////////////////////// BUTTON /////////////////////////////////

typedef struct {
	const char* text;
	b8 pressed;
} Button;

b8 gui_button(const char* text, u64 flags)
{
	u32 type = gui->register_ids.widget_button;
	
	u64 id = gui_write_widget(type, flags, (u64)text);

	b8 res = FALSE;

	GuiWidget* widget = gui_find_widget(type, id, gui_current_parent());
	if (widget) {

		Button* button = (Button*)(widget + 1);
		res = button->pressed;
	}

	gui_write_text(text);

	return res;
}

static u8* button_read(GuiWidget* widget, u8* it)
{
	Button* button = (Button*)(widget + 1);
	button->pressed = FALSE;

	gui_read_text(it, button->text);
	
	return it;
}

static void button_update(GuiParent* parent, GuiWidget* widget, b8 has_focus)
{
	Button* button = (Button*)(widget + 1);

	if (has_focus) {

		if (input_mouse_button(MouseButton_Left, InputState_Released)) {
			
			if (gui_mouse_in_bounds(widget->bounds)) {
				button->pressed = TRUE;
			}

			gui_free_focus();
		}
	}
	else {
		if (input_mouse_button(MouseButton_Left, InputState_Pressed) && gui_mouse_in_bounds(widget->bounds)) {

			gui_set_focus(widget, parent->id);
		}
	}
}

static void button_draw(GuiWidget* widget)
{
	Button* button = (Button*)(widget + 1);

	Color color = color_gray(100);

	if (gui->focus.type == widget->type && gui->focus.id == widget->id) {

		color = color_red();
	}
	else if (gui_mouse_in_bounds(widget->bounds)) {

		color = color_gray(175);
	}

	gui_draw_bounds(widget->bounds, color);
	gui_draw_text(button->text, widget->bounds, TextAlignment_Center);
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
	imgui_write_layout_update(type);

	u8 update = 0;
	gui_write(update);
	gui_write_(&data, sizeof(data));
}

void gui_stack_layout_update_size(f32 width, f32 height)
{
	u32 type = gui->register_ids.layout_stack;
	imgui_write_layout_update(type);

	u8 update = 1;
	gui_write(update);
	gui_write(width);
	gui_write(height);
}

static void stack_layout_initialize(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiStackLayoutInternal* d = (GuiStackLayoutInternal*)layout->data;
	d->data.width = 0.9f;
	d->data.height = 0.05f;
	d->data.margin = 0.03f;
	d->data.padding = 0.01f;
	d->offset = d->data.margin;
}

static v4 stack_layout_compute_bounds(GuiParent* parent)
{
	GuiLayout* layout = &parent->layout;
	GuiStackLayoutInternal* d = (GuiStackLayoutInternal*)layout->data;

	f32 y = 1.f - d->offset - d->data.height * 0.5f;
	d->offset += d->data.height + d->data.padding;
	
	return v4_set(0.5f, y, d->data.width, d->data.height);
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

static void register_default_widgets()
{
	gui->widget_registers[0].read_fn = button_read;
	gui->widget_registers[0].update_fn = button_update;
	gui->widget_registers[0].draw_fn = button_draw;
	gui->widget_registers[0].size = sizeof(Button);
	gui->register_ids.widget_button = 0;

	gui->widget_register_count = 1;
}

static void register_default_layouts()
{
	gui->layout_registers[0].initialize_fn = stack_layout_initialize;
	gui->layout_registers[0].compute_bounds_fn = stack_layout_compute_bounds;
	gui->layout_registers[0].update_fn = stack_layout_update;
	string_copy(gui->layout_registers[0].name, "stack", NAME_SIZE);
	gui->register_ids.layout_stack = 0;

	gui->layout_register_count = 1;
}
