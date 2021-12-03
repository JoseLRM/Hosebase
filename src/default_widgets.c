#include "Hosebase/imgui.h"

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

/////////////////////////////////////////////// DRAWABLE ////////////////////////////////////////////////////////////

static u32 DRAWABLE_TYPE;

typedef struct {
	GuiDrawableFn fn;
} Drawable;

void gui_drawable(GuiDrawableFn fn, u64 flags)
{
	if (fn == NULL)
		return;

	gui_write_widget(DRAWABLE_TYPE, flags, (u64)fn);
	gui_write(fn);
}

static u8* drawable_read(GuiWidget* widget, u8* it)
{
	Drawable* drawable = (Drawable*)(widget + 1);
	gui_read(it, drawable->fn);
	return it;
}

static void drawable_draw(GuiWidget* widget)
{
	Drawable* drawable = (Drawable*)(widget + 1);
	drawable->fn(widget);
}

static void register_default_widgets()
{
	GuiRegisterWidgetDesc desc;

	desc.name = "button";
	desc.read_fn = button_read;
	desc.update_fn = button_update;
	desc.draw_fn = button_draw;
	desc.size = sizeof(Button);
	BUTTON_TYPE = gui_register_widget(&desc);

	desc.name = "slider";
	desc.read_fn = slider_read;
	desc.update_fn = slider_update;
	desc.draw_fn = slider_draw;
	desc.size = sizeof(Slider);
	SLIDER_TYPE = gui_register_widget(&desc);

	desc.name = "drawable";
	desc.read_fn = drawable_read;
	desc.update_fn = NULL;
	desc.draw_fn = drawable_draw;
	desc.size = sizeof(Drawable);
	DRAWABLE_TYPE = gui_register_widget(&desc);
}
