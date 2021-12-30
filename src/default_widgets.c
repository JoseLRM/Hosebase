#include "Hosebase/imgui.h"

///////////////////////////////// TEXT /////////////////////////////////

static u32 TEXT_TYPE;

typedef struct {
	const char* text;
} Text;

void gui_text(const char* text, ...)
{
	u64 id = (u64)text;

	id = gui_write_widget(TEXT_TYPE, 0, id);

	va_list args;
	va_start(args, text);

	// TODO: Make it safer
	char txt[10000];

	vsprintf(txt, text, args);
	gui_write_text(txt);

	va_end(args);
}

static u8* text_read(GuiWidget* widget, u8* it)
{
	Text* text = (Text*)(widget + 1);

	gui_read_text(it, text->text);

	return it;
}

static void text_update(GuiParent* parent, GuiWidget* widget, b8 has_focus)
{
}

static void text_draw(GuiWidget* widget)
{
	Text* text = (Text*)(widget + 1);
	gui_draw_text(text->text, widget->bounds, TextAlignment_Left);
}

///////////////////////////////// BUTTON /////////////////////////////////

static u32 BUTTON_TYPE;

typedef struct {
	const char* text;
	b8 pressed;
} Button;

b8 gui_button(const char* text, u64 flags)
{
	u64 id;

	{
		text = string_validate(text);
		if (text[0] == '#') {
			id = 0x8ff42a8d34;
			text++;
		}
		else id = (u64)text;
	}
	
	id = gui_write_widget(BUTTON_TYPE, flags, id);

	b8 res = FALSE;

	GuiWidget* widget = gui_find_widget(BUTTON_TYPE, id, gui_current_parent());
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

			gui_set_focus(widget, parent->id, 0);
		}
	}
}

static void button_draw(GuiWidget* widget)
{
	Button* button = (Button*)(widget + 1);

	Color color = color_gray(100);

	GuiFocus focus = gui_get_focus();

	if (focus.type == widget->type && focus.id == widget->id) {

		color = color_red();
	}
	else if (gui_mouse_in_bounds(widget->bounds)) {

		color = color_gray(175);
	}

	gui_draw_bounds(widget->bounds, color);
	gui_draw_text(button->text, widget->bounds, TextAlignment_Center);
}

///////////////////////////////// TEXT FIELD /////////////////////////////////

// TODO
static char text_field_text[300];
static TextContext text_field_context;

static u32 TEXT_FIELD_TYPE;

typedef struct {
	const char* buffer;
	u32 buffer_size;
	b8 modified;
} TextField;

b8 gui_text_field(char* buffer, u32 buffer_size, u64 flags)
{
	u64 id = gui_write_widget(TEXT_FIELD_TYPE, flags, 0x29c30a87645);

	b8 res = FALSE;

	GuiWidget* widget = gui_find_widget(TEXT_FIELD_TYPE, id, gui_current_parent());
	if (widget) {

		TextField* field = (TextField*)(widget + 1);
		res = field->modified;

		if (res) {
			string_copy(buffer, text_field_text, buffer_size);
		}
	}

	gui_write_text(buffer);
	gui_write(buffer_size);

	return res;
}

static u8* text_field_read(GuiWidget* widget, u8* it)
{
	TextField* field = (TextField*)(widget + 1);
	field->modified = FALSE;

	gui_read_text(it, field->buffer);
	gui_read(it, field->buffer_size);

	return it;
}

static void text_field_update(GuiParent* parent, GuiWidget* widget, b8 has_focus)
{
	TextField* field = (TextField*)(widget + 1);

	if (has_focus) {

		if (input_key(Key_Enter, InputState_Pressed)) {
			gui_free_focus();
		}

		else if (input_mouse_button(MouseButton_Left, InputState_Pressed)) {

			if (!gui_mouse_in_bounds(widget->bounds)) {
				gui_free_focus();
			}
		}

		else {

			u64 flags = 0;

			TextProcessDesc desc;
			desc.buffer = text_field_text;
			desc.buffer_size = 300;
			desc.context = &text_field_context;
			desc.bounds = widget->bounds;
			desc.font = gui_font();
			desc.font_size = 1.f;
			desc.in_flags = 0;
			desc.out_flags = &flags;

			text_process(&desc);

			// TODO:
			field->modified = TRUE;
		}
	}
	else {
		if (input_mouse_button(MouseButton_Left, InputState_Pressed) && gui_mouse_in_bounds(widget->bounds)) {

			gui_set_focus(widget, parent->id, 0);
			string_copy(text_field_text, field->buffer, 300);
		}
	}
}

static void text_field_draw(GuiWidget* widget)
{
	TextField* field = (TextField*)(widget + 1);

	GuiFocus focus = gui_get_focus();

	const char* text = field->buffer;

	if (focus.type == widget->type && focus.id == widget->id) {

		text = text_field_text;
	}

	gui_draw_text(text, widget->bounds, TextAlignment_Left);
}

////////////////////////////////////////////// SLIDER ///////////////////////////////////////////

static u32 SLIDER_TYPE;

typedef struct {
	const char* text;

	GuiSliderType type;
	union {
		struct {
			f32 n, min, max;
		} _f32;
		struct {
			u32 n, min, max;
		} _u32;
	};

	b8 in_focus;
} Slider;

b8 gui_slider_ex(const char* text, void* n, const void* min, const void* max, GuiSliderType type, u64 flags)
{
	u64 id;

	{
		text = string_validate(text);
		if (text[0] == '#') {
			id = 0x8ff42a8d34;
			text++;
		}
		else id = (u64)text;
	}

	id = gui_write_widget(SLIDER_TYPE, flags, id);

	b8 res = FALSE;

	// TODO
	u32 type_size = 4;

	GuiWidget* widget = gui_find_widget(SLIDER_TYPE, id, gui_current_parent());
	if (widget) {

		Slider* slider = (Slider*)(widget + 1);
		
		if (slider->in_focus) {

			switch (type)
			{
			case GuiSliderType_f32:
			case GuiSliderType_u32:
				memory_copy(n, &slider->_f32.n, 4);
				break;
			}

			res = TRUE;
		}
	}

	gui_write_text(text);
	gui_write(type);
	gui_write_(n, type_size);
	gui_write_(min, type_size);
	gui_write_(max, type_size);

	switch (type)
	{

	case GuiSliderType_f32:
	{
		f32 _min = *(f32*)min;
		f32 _max = *(f32*)max;
		f32 _n = *(f32*)n;

		_n = math_clamp(_min, _n, _max);

		memory_copy(n, &_n, type_size);
	}
	break;

	case GuiSliderType_u32:
	{
		u32 _min = *(u32*)min;
		u32 _max = *(u32*)max;
		u32 _n = *(u32*)n;

		_n = SV_MAX(SV_MIN(_n, _max), _min);

		memory_copy(n, &_n, type_size);
	}
	break;

	}

	return res;
}

static u8* slider_read(GuiWidget* widget, u8* it)
{
	Slider* slider = (Slider*)(widget + 1);
	slider->in_focus = FALSE;

	gui_read_text(it, slider->text);
	gui_read(it, slider->type);

	switch (slider->type) {

	case GuiSliderType_f32:
		gui_read(it, slider->_f32.n);
		gui_read(it, slider->_f32.min);
		gui_read(it, slider->_f32.max);
		break;

	case GuiSliderType_u32:
		gui_read(it, slider->_u32.n);
		gui_read(it, slider->_u32.min);
		gui_read(it, slider->_u32.max);
		break;

	}

	return it;
}

static void slider_update(GuiParent* parent, GuiWidget* widget, b8 has_focus)
{
	Slider* slider = (Slider*)(widget + 1);

	slider->in_focus = has_focus;

	if (has_focus) {

		v2 mouse = gui_mouse_position();
		f32 pos = mouse.x;

		v4 b = widget->bounds;
		pos = (pos - (b.x - (b.z * 0.5f))) / b.z;

		f32 n = math_clamp01(pos);

		switch (slider->type) {

		case GuiSliderType_f32:
		{
			f32 max = slider->_f32.max;
			f32 min = slider->_f32.min;
			n *= max - min;
			n += min;

			slider->_f32.n = n;
		}
		break;

		case GuiSliderType_u32:
		{
			u32 max = slider->_u32.max;
			u32 min = slider->_u32.min;
			n *= (f32)(max - min);
			n += min;

			slider->_u32.n = n;
		}
		break;

		}

		if (input_mouse_button(MouseButton_Left, InputState_Released)) {
			gui_free_focus();
		}
	}
	else {
		if (input_mouse_button(MouseButton_Left, InputState_Pressed) && gui_mouse_in_bounds(widget->bounds)) {

			gui_set_focus(widget, parent->id, 0);
		}
	}
}

static void slider_draw(GuiWidget* widget)
{
	Slider* slider = (Slider*)(widget + 1);

	Color bg_color = color_gray(40);
	Color color = color_gray(100);

	GuiFocus focus = gui_get_focus();

	if (focus.type == widget->type && focus.id == widget->id) {

		color = color_red();
	}
	else if (gui_mouse_in_bounds(widget->bounds)) {

		color = color_gray(175);
	}

	// TODO: Use text properly

	v4 b = widget->bounds;
	gui_draw_bounds(b, bg_color);

	v2 pixel = gui_pixel_size();

	const f32 outline_size = 3.f;
	b.z -= outline_size * 2.f * pixel.x;
	b.w -= outline_size * 2.f * pixel.y;

	f32 n = 0.f;

	switch (slider->type) {

	case GuiSliderType_f32:
		n = (slider->_f32.n - slider->_f32.min) / (slider->_f32.max - slider->_f32.min);
		break;

	case GuiSliderType_u32:
		n = (f32)(slider->_u32.n - slider->_u32.min) / (f32)(slider->_u32.max - slider->_u32.min);
		break;

	}

	b.x -= b.z * 0.5f;
	b.z *= n;
	b.x += b.z * 0.5f;

	gui_draw_bounds(b, color);
	gui_draw_text(slider->text, widget->bounds, TextAlignment_Center);
}

///////////////////////////////// CHECKBOX /////////////////////////////////

static u32 CHECKBOX_TYPE;

typedef struct {
	const char* text;
	b8 value;
	b8 pressed;
} Checkbox;

b8 gui_checkbox(const char* text, b8* n, u64 flags)
{
	u64 id;

	{
		text = string_validate(text);
		if (text[0] == '#') {
			id = 0x8ff42a8d34;
			text++;
		}
		else id = (u64)text;
	}

	id = gui_write_widget(CHECKBOX_TYPE, flags, id);

	b8 res = FALSE;

	GuiWidget* widget = gui_find_widget(CHECKBOX_TYPE, id, gui_current_parent());
	if (widget) {

		Checkbox* cb = (Checkbox*)(widget + 1);
		res = cb->pressed;

		if (cb->pressed) {
			*n = cb->value;
		}
	}

	gui_write_text(text);
	gui_write(*n);

	return res;
}

inline v4 compute_checkbox_button(v4 bounds)
{
	v4 b = bounds;
	b.x -= b.z * 0.5f;
	b.w *= 0.8f;
	b.z = b.w / gui_aspect();

	b.x += b.z * 0.5f;
	return b;
}

inline v4 compute_text_bounds(v4 bounds)
{
	f32 button_width = (bounds.w * 0.8f) / gui_aspect();

	v4 b = bounds;
	b.x += b.z * 0.5f;
	b.z -= button_width * 1.1f;
	b.x -= b.z * 0.5f;

	return b;
}

static u8* checkbox_read(GuiWidget* widget, u8* it)
{
	Checkbox* cb = (Checkbox*)(widget + 1);
	cb->pressed = FALSE;

	gui_read_text(it, cb->text);
	gui_read(it, cb->value);

	return it;
}

static void checkbox_update(GuiParent* parent, GuiWidget* widget, b8 has_focus)
{
	Checkbox* cb = (Checkbox*)(widget + 1);

	v4 button = compute_checkbox_button(widget->bounds);

	if (has_focus) {

		if (input_mouse_button(MouseButton_Left, InputState_Released)) {

			if (gui_mouse_in_bounds(button)) {
				cb->pressed = TRUE;
				cb->value = !cb->value;
			}

			gui_free_focus();
		}
	}
	else {
		if (input_mouse_button(MouseButton_Left, InputState_Pressed) && gui_mouse_in_bounds(button)) {

			gui_set_focus(widget, parent->id, 0);
		}
	}
}

static void checkbox_draw(GuiWidget* widget)
{
	Checkbox* cb = (Checkbox*)(widget + 1);

	Color button_color = color_gray(100);
	Color button_decoration_color = color_red();

	GuiFocus focus = gui_get_focus();

	v4 button = compute_checkbox_button(widget->bounds);

	if (focus.type == widget->type && focus.id == widget->id) {

		button_color = color_gray(75);
	}
	else if (gui_mouse_in_bounds(button)) {

		button_color = color_gray(175);
	}

	gui_draw_bounds(button, button_color);

	if (cb->value) {
		f32 decoration_size = 0.8f;

		button.z *= decoration_size;
		button.w *= decoration_size;

		gui_draw_bounds(button, button_decoration_color);
	}

	gui_draw_text(cb->text, compute_text_bounds(widget->bounds), TextAlignment_Left);
}

/////////////////////////////////////////////// DRAWABLE ////////////////////////////////////////////////////////////

static u32 DRAWABLE_TYPE;

typedef struct {
	GuiDrawableFn fn;
	const void* data;
} Drawable;

void gui_drawable(GuiDrawableFn fn, const void* data, u32 size, u64 flags)
{
	if (fn == NULL)
		return;

	gui_write_widget(DRAWABLE_TYPE, flags, (u64)fn);
	gui_write(fn);
	gui_write(size);
	gui_write_(data, size);
}

static u8* drawable_read(GuiWidget* widget, u8* it)
{
	Drawable* drawable = (Drawable*)(widget + 1);
	drawable->data = NULL;
	u32 size;

	gui_read(it, drawable->fn);
	gui_read(it, size);

	if (size) {
		drawable->data = gui_read_buffer(it, size);
	}

	return it;
}

static void drawable_draw(GuiWidget* widget)
{
	Drawable* drawable = (Drawable*)(widget + 1);
	drawable->fn(widget, drawable->data);
}

static void register_default_widgets()
{
	GuiRegisterWidgetDesc desc;

	desc.name = "text";
	desc.read_fn = text_read;
	desc.update_fn = text_update;
	desc.draw_fn = text_draw;
	desc.size = sizeof(Text);
	TEXT_TYPE = gui_register_widget(&desc);

	desc.name = "button";
	desc.read_fn = button_read;
	desc.update_fn = button_update;
	desc.draw_fn = button_draw;
	desc.size = sizeof(Button);
	BUTTON_TYPE = gui_register_widget(&desc);

	desc.name = "text_field";
	desc.read_fn = text_field_read;
	desc.update_fn = text_field_update;
	desc.draw_fn = text_field_draw;
	desc.size = sizeof(TextField);
	TEXT_FIELD_TYPE = gui_register_widget(&desc);

	desc.name = "slider";
	desc.read_fn = slider_read;
	desc.update_fn = slider_update;
	desc.draw_fn = slider_draw;
	desc.size = sizeof(Slider);
	SLIDER_TYPE = gui_register_widget(&desc);

	desc.name = "checkbox";
	desc.read_fn = checkbox_read;
	desc.update_fn = checkbox_update;
	desc.draw_fn = checkbox_draw;
	desc.size = sizeof(Checkbox);
	CHECKBOX_TYPE = gui_register_widget(&desc);

	desc.name = "drawable";
	desc.read_fn = drawable_read;
	desc.update_fn = NULL;
	desc.draw_fn = drawable_draw;
	desc.size = sizeof(Drawable);
	DRAWABLE_TYPE = gui_register_widget(&desc);
}
