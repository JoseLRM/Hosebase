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

			gui_set_focus(widget, parent->id);
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

	desc.name = "drawable";
	desc.read_fn = drawable_read;
	desc.update_fn = NULL;
	desc.draw_fn = drawable_draw;
	desc.size = sizeof(Drawable);
	DRAWABLE_TYPE = gui_register_widget(&desc);
}
