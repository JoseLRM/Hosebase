#pragma once

#include "Hosebase/math.h"

SV_BEGIN_C_HEADER

typedef enum {
	ControllerType_KeyboardAndMouse,
	ControllerType_Gamepad,
	ControllerType_TouchScreen,
} ControllerType;

typedef enum {
	GamepadButton_X,
	GamepadButton_Y,
	GamepadButton_A,
	GamepadButton_B,
	GamepadButton_Up,
	GamepadButton_Down,
	GamepadButton_Left,
	GamepadButton_Right,
	GamepadButton_Start,
	GamepadButton_Back,
	GamepadButton_MaxEnum,
} GamepadButton;

typedef enum {
	MouseButton_Left,
	MouseButton_Right,
	MouseButton_Center,

	MouseButton_MaxEnum,
	MouseButton_None,
} MouseButton;

typedef enum {
	Key_Tab,
	Key_Shift,
	Key_Control,
	Key_Capital,
	Key_Escape,
	Key_Alt,
	Key_Space,
	Key_Left,
	Key_Up,
	Key_Right,
	Key_Down,
	Key_Enter,
	Key_Insert,
	Key_Delete,
	Key_Supr,
	Key_Begin,
	Key_End,
	Key_A,
	Key_B,
	Key_C,
	Key_D,
	Key_E,
	Key_F,
	Key_G,
	Key_H,
	Key_I,
	Key_J,
	Key_K,
	Key_L,
	Key_M,
	Key_N,
	Key_O,
	Key_P,
	Key_Q,
	Key_R,
	Key_S,
	Key_T,
	Key_U,
	Key_V,
	Key_W,
	Key_X,
	Key_Y,
	Key_Z,
	Key_Num0,
	Key_Num1,
	Key_Num2,
	Key_Num3,
	Key_Num4,
	Key_Num5,
	Key_Num6,
	Key_Num7,
	Key_Num8,
	Key_Num9,
	Key_F1,
	Key_F2,
	Key_F3,
	Key_F4,
	Key_F5,
	Key_F6,
	Key_F7,
	Key_F8,
	Key_F9,
	Key_F10,
	Key_F11,
	Key_F12,
	Key_F13,
	Key_F14,
	Key_F15,
	Key_F16,
	Key_F17,
	Key_F18,
	Key_F19,
	Key_F20,
	Key_F21,
	Key_F22,
	Key_F23,
	Key_F24,

	Key_MaxEnum,
	Key_None,
} Key;

typedef enum {
	InputState_None,
	InputState_Pressed,
	InputState_Hold,
	InputState_Released,
	InputState_Any,
} InputState;

typedef enum {
	TextCommand_Null,
	TextCommand_DeleteLeft,
	TextCommand_DeleteRight,
	TextCommand_Left,
	TextCommand_Right,
	TextCommand_Up,
	TextCommand_Down,
	TextCommand_Enter,
	TextCommand_Escape,
	TextCommand_Begin,
	TextCommand_End,
	TextCommand_Copy,
	TextCommand_Cut,
	TextCommand_Paste,
} TextCommand;

b8 input_any();
b8 input_focus();

b8 input_key(Key key, InputState input_state);
b8 input_mouse_button(MouseButton mouse_button, InputState input_state);
b8 input_gamepad_button(GamepadButton gamepad_button, InputState input_state);
v2 input_gamepad_joystick_left();
v2 input_gamepad_joystick_right();

f32 input_mouse_wheel();
v2  input_mouse_position();
v2  input_mouse_dragging();

v2 input_touch_position(u32 index);
b8 input_touch_button(InputState input_state, u32 index);

const char* input_text();
u32 input_text_command_count();
TextCommand input_text_command(u32 index);

ControllerType input_last_controller_used();

// Utils

InputState input_state_update(InputState input_state, b8 press);

// Internal

b8   _input_initialize();
void _input_close();
void _input_update();

void _input_key_set_pressed(Key key);
void _input_key_set_released(Key key);
void _input_mouse_button_set_pressed(MouseButton mouse_button);
void _input_mouse_button_set_released(MouseButton mouse_button);

void _input_text_command_add(TextCommand text_command);
void _input_text_add(char c);

void _input_mouse_wheel_set(f32 value);
void _input_mouse_position_set(v2 value);
void _input_mouse_dragging_set(v2 value);

void _input_gamepad_press(GamepadButton button);
void _input_gamepad_set_joystick(b8 left, v2 value);

void _input_touch_set(InputState state, v2 position, u32 id);

SV_END_C_HEADER