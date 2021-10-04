#include "Hosebase/input.h"

#define INPUT_TEXT_SIZE 20

typedef struct {

	InputState keys[Key_MaxEnum];
	InputState mouse_buttons[MouseButton_MaxEnum];

	char text[INPUT_TEXT_SIZE];
	// TODO: static buffer
	DynamicArray(TextCommand) text_commands;

	v2	mouse_position;
	v2	mouse_last_position;
	v2	mouse_dragging;
	f32	mouse_wheel;

	b8 unused;

} InputData;

static InputData* input = NULL;

static b8 validate_input_state(InputState actual, InputState input_state)
{
	if (input_state == InputState_Any) {

		return actual == InputState_Pressed || actual == InputState_Hold || actual == InputState_Released;
	}
	else return actual == input_state;
}

b8 input_key(Key key, InputState input_state)
{
	InputState actual = input->keys[key];
	return validate_input_state(actual, input_state);
}

b8 input_mouse_button(MouseButton mouse_button, InputState input_state)
{
	InputState actual = input->mouse_buttons[mouse_button];
	return validate_input_state(actual, input_state);
}

f32 input_mouse_wheel()
{
	return input->mouse_wheel;
}

v2 input_mouse_position()
{
	return input->mouse_position;
}

v2 input_mouse_dragging()
{
	return input->mouse_dragging;
}

const char* input_text()
{
	return input->text;
}

u32 input_text_command_count()
{
	return input->text_commands.size;
}

TextCommand input_text_command(u32 index)
{
	return *(TextCommand*)array_get(&input->text_commands, index);
}

b8 _input_initialize()
{
	input = memory_allocate(sizeof(InputData));

	input->text_commands = array_init(TextCommand, 1.1f);

	return TRUE;
}

void _input_close()
{
	if (input) {

		array_close(&input->text_commands);
		
		memory_free(input);
	}
}

void _input_update()
{
	foreach(i, Key_MaxEnum) {

		InputState* state = input->keys + i;
		
		if (*state == InputState_Pressed) {
			*state = InputState_Hold;
		}
		else if (*state == InputState_Released) {
			*state = InputState_None;
		}
	}

	foreach(i, MouseButton_MaxEnum) {

		InputState* state = input->mouse_buttons + i;
		
		if (*state == InputState_Pressed) {
			*state = InputState_Hold;
		}
		else if (*state == InputState_Released) {
			*state = InputState_None;
		}
	}

	input->mouse_last_position = input->mouse_position;
	input->text[0] = '\0';
	array_reset(&input->text_commands);
	input->mouse_wheel = 0.f;
}

void _input_key_set_pressed(Key key)
{
	input->keys[key] = InputState_Pressed;
}

void _input_key_set_released(Key key)
{
	input->keys[key] = InputState_Released;
}

void _input_mouse_button_set_pressed(MouseButton mouse_button)
{
	input->mouse_buttons[mouse_button] = InputState_Pressed;
}

void _input_mouse_button_set_released(MouseButton mouse_button)
{
	input->mouse_buttons[mouse_button] = InputState_Released;
}

void _input_text_command_add(TextCommand text_command)
{
	array_push(&input->text_commands, text_command);
}

void _input_text_add(const char* text)
{
	string_append(input->text, text, INPUT_TEXT_SIZE);
}

void _input_mouse_wheel_set(f32 value)
{
	input->mouse_wheel = value;
}

void _input_mouse_position_set(v2 value)
{
	input->mouse_position = value;
}

void _input_mouse_dragging_set(v2 value)
{
	input->mouse_dragging = value;
}