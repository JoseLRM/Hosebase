#include "Hosebase/input.h"
#include "Hosebase/platform.h"

#define INPUT_TEXT_SIZE 20

typedef struct
{

	b8 any;

	InputState keys[Key_MaxEnum];
	InputState mouse_buttons[MouseButton_MaxEnum];

	char text[INPUT_TEXT_SIZE];
	TextCommand text_commands[INPUT_TEXT_SIZE];
	u32 text_command_count;

	v2 mouse_position;
	v2 mouse_last_position;
	v2 mouse_dragging;
	f32 mouse_wheel;

	Key release_keys[Key_MaxEnum];
	u32 release_key_count;
	Key release_mouse_buttons[MouseButton_MaxEnum];
	u32 release_mouse_button_count;

	InputState gamepad_button_state[GamepadButton_MaxEnum];
	v2 joystick_left;
	v2 joystick_right;

	InputState touch_state[5];
	v2 touch_position[5];
	i32 touch_id[5];

	ControllerType last_controller_used;

} InputData;

static InputData *input = NULL;

static b8 validate_input_state(InputState actual, InputState input_state)
{
	if (input_state == InputState_Any)
	{

		return actual == InputState_Pressed || actual == InputState_Hold || actual == InputState_Released;
	}
	else
		return actual == input_state;
}

b8 input_any()
{
	return input->any;
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

b8 input_gamepad_button(GamepadButton gamepad_button, InputState input_state)
{
	InputState actual = input->gamepad_button_state[gamepad_button];
	return validate_input_state(actual, input_state);
}

v2 input_gamepad_joystick_left()
{
	return input->joystick_left;
}

v2 input_gamepad_joystick_right()
{
	return input->joystick_right;
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

v2 input_touch_position(u32 index)
{
	if (index >= SV_ARRAY_SIZE(input->touch_position))
		return v2_zero();
	return input->touch_position[index];
}

b8 input_touch_button(InputState input_state, u32 index)
{
	if (index >= SV_ARRAY_SIZE(input->touch_state))
		return FALSE;

	return validate_input_state(input->touch_state[index], input_state);
}

const char *input_text()
{
	return input->text;
}

u32 input_text_command_count()
{
	return input->text_command_count;
}

TextCommand input_text_command(u32 index)
{
	if (index >= input->text_command_count)
		return TextCommand_Null;
	return input->text_commands[index];
}

ControllerType input_last_controller_used()
{
	return input->last_controller_used;
}

InputState input_state_update(InputState input_state, b8 press)
{
	if (press)
	{
		if (input_state == InputState_Pressed)
			input_state = InputState_Hold;
		else if (input_state == InputState_Released || input_state == InputState_None)
			input_state = InputState_Pressed;
	}
	else
	{
		if (input_state == InputState_Pressed || input_state == InputState_Hold)
			input_state = InputState_Released;
		else if (input_state == InputState_Released)
			input_state = InputState_None;
	}

	return input_state;
}

b8 _input_initialize()
{
	input = memory_allocate(sizeof(InputData));

#if SV_PLATFORM_ANDROID
	input->last_controller_used = ControllerType_TouchScreen;
#endif

	foreach (i, SV_ARRAY_SIZE(input->touch_id))
		input->touch_id[i] = -1;

	return TRUE;
}

void _input_close()
{
	if (input)
	{
		memory_free(input);
	}
}

void _input_update()
{
	input->any = FALSE;

	foreach (i, Key_MaxEnum)
	{

		InputState *state = input->keys + i;

		if (*state == InputState_Pressed)
		{
			*state = InputState_Hold;
			input->any = TRUE;
		}
		else if (*state == InputState_Hold)
		{
			input->any = TRUE;
		}
		else if (*state == InputState_Released)
		{
			*state = InputState_None;
		}
	}

	foreach (i, input->release_key_count)
	{

		Key key = input->release_keys[i];
		input->keys[key] = InputState_Released;
	}
	input->release_key_count = 0;

	foreach (i, MouseButton_MaxEnum)
	{

		InputState *state = input->mouse_buttons + i;

		if (*state == InputState_Pressed)
		{
			*state = InputState_Hold;
			input->any = TRUE;
		}
		else if (*state == InputState_Hold)
		{
			input->any = TRUE;
		}
		else if (*state == InputState_Released)
		{
			*state = InputState_None;
		}
	}

	foreach (i, input->release_mouse_button_count)
	{

		MouseButton button = input->release_mouse_buttons[i];
		input->mouse_buttons[button] = InputState_Released;
	}
	input->release_mouse_button_count = 0;

	input->mouse_last_position = input->mouse_position;
	input->text[0] = '\0';
	input->text_command_count = 0;
	input->mouse_wheel = 0.f;

	// Gamepad
	{
		foreach (i, GamepadButton_MaxEnum)
		{
			InputState *state = input->gamepad_button_state + i;

			if (*state == InputState_Released)
			{
				*state = InputState_None;
			}
			else if (*state == InputState_Pressed || *state == InputState_Hold)
			{
				*state = InputState_Released;
			}
		}

		input->joystick_left = v2_zero();
		input->joystick_right = v2_zero();
	}

	// DEBUG: Print touch data
	{
		foreach (i, SV_ARRAY_SIZE(input->touch_state))
		{
			if (input->touch_id[i] == -1)
				continue;

			InputState state = input->touch_state[i];
			v2 pos = input->touch_position[i];

			const char *str = "None";
			if (state == InputState_Hold)
				str = "Hold";
			else if (state == InputState_Pressed)
				str = "Pressed";
			else if (state == InputState_Released)
				str = "Released";

			if (state != InputState_Hold)
				SV_LOG_INFO("%u: %s\n", i, str);
		}
	}

	// Touch screen
	foreach (i, SV_ARRAY_SIZE(input->touch_state))
	{
		if (input->touch_state[i] == InputState_Released)
		{
			input->touch_state[i] = InputState_None;
			input->touch_id[i] = -1;
		}
		else if (input->touch_state[i] == InputState_Pressed)
		{
			input->any = TRUE;
			input->touch_state[i] = InputState_Hold;
		}
		else if (input->touch_state[i] == InputState_Hold)
		{
			input->any = TRUE;
		}
	}
}

void _input_key_set_pressed(Key key)
{
	input->keys[key] = InputState_Pressed;
	input->any = TRUE;
	input->last_controller_used = ControllerType_KeyboardAndMouse;
}

void _input_key_set_released(Key key)
{
	if (input->keys[key] == InputState_Pressed && input->release_key_count < Key_MaxEnum)
	{

		input->release_keys[input->release_key_count++] = key;
	}
	else
		input->keys[key] = InputState_Released;

	input->any = TRUE;
	input->last_controller_used = ControllerType_KeyboardAndMouse;
}

void _input_mouse_button_set_pressed(MouseButton mouse_button)
{
	input->mouse_buttons[mouse_button] = InputState_Pressed;
	input->any = TRUE;
	input->last_controller_used = ControllerType_KeyboardAndMouse;
}

void _input_mouse_button_set_released(MouseButton mouse_button)
{
	if (input->mouse_buttons[mouse_button] == InputState_Pressed && input->release_mouse_button_count < MouseButton_MaxEnum)
	{

		input->release_mouse_buttons[input->release_mouse_button_count++] = mouse_button;
	}
	else
		input->mouse_buttons[mouse_button] = InputState_Released;

	input->any = TRUE;
	input->last_controller_used = ControllerType_KeyboardAndMouse;
}

void _input_text_command_add(TextCommand text_command)
{
	if (input->text_command_count >= SV_ARRAY_SIZE(input->text_commands))
	{
		assert_title(FALSE, "Input text command buffer overflow");
		return;
	}

	input->text_commands[input->text_command_count++] = text_command;
	input->any = TRUE;
}

void _input_text_add(char c)
{
	char str[2];
	str[0] = c;
	str[1] = '\0';
	string_append(input->text, str, INPUT_TEXT_SIZE);
	input->any = TRUE;
}

void _input_mouse_wheel_set(f32 value)
{
	input->mouse_wheel = value;
	input->any = TRUE;

	if (fabs(value) > 0.00001f)
	{
		input->last_controller_used = ControllerType_KeyboardAndMouse;
	}
}

void _input_mouse_position_set(v2 value)
{
	input->mouse_position = value;
}

void _input_mouse_dragging_set(v2 value)
{
	input->mouse_dragging = value;

	if (fabs(value.x) > 0.0001f || fabs(value.y) > 0.0001f)
		input->any = TRUE;
}

void _input_gamepad_press(GamepadButton button)
{
	InputState state = input->gamepad_button_state[button];

	if (state == InputState_None)
	{
		input->gamepad_button_state[button] = InputState_Pressed;
	}
	else if (state == InputState_Released)
	{
		input->gamepad_button_state[button] = InputState_Hold;
	}

	input->last_controller_used = ControllerType_Gamepad;
}

void _input_gamepad_set_joystick(b8 left, v2 value)
{
	if (fabs(value.x) < 0.0001f)
		value.x = 0.f;

	if (fabs(value.y) < 0.0001f)
		value.y = 0.f;

	if (left)
		input->joystick_left = value;
	else
		input->joystick_right = value;
}

static u32 get_touch_index(u32 id)
{
	u32 free_index = u32_max;
	foreach (i, SV_ARRAY_SIZE(input->touch_id))
	{
		if (input->touch_id[i] == id)
			return i;
		else if (free_index == u32_max && input->touch_id[i] == -1)
			free_index = i;
	}

	return free_index;
}

void _input_touch_set(InputState state, v2 position, u32 id)
{
	u32 index = get_touch_index(id);

	if (index == u32_max)
		return;

	input->touch_state[index] = (state == InputState_None) ? input->touch_state[index] : state;
	input->touch_position[index] = position;
	input->touch_id[index] = id;
	input->last_controller_used = ControllerType_TouchScreen;
}