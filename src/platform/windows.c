#if SV_PLATFORM_WINDOWS

#include "Hosebase/hosebase.h"

#define NOMINMAX
#include "windows.h"
#include "Shlobj.h"
#include "xinput.h"

#undef near
#undef far

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Xinput.lib")

#define WRITE_BARRIER \
	_WriteBarrier();  \
	_mm_sfence()
#define READ_BARRIER _ReadBarrier()
#define GENERAL_BARRIER \
	WRITE_BARRIER;      \
	READ_BARRIER

#include "time.h"

#include "Hosebase/platform.h"
#include "Hosebase/input.h"

#include "Hosebase/graphics.h"

#define TASK_QUEUE_SIZE 6000
#define TASK_THREAD_MAX 64

static b8 _task_initialize();
static void _task_close();

typedef struct
{

	HANDLE thread;
	u32 id;

} TaskThreadData;

typedef struct
{

	TaskContext *context;
	void *fn;
	b8 user_data[TASK_DATA_SIZE];
	u8 type;

} TaskData;

typedef struct
{

	TaskData tasks[TASK_QUEUE_SIZE];
	volatile u32 task_count;
	volatile u32 task_completed;
	volatile u32 task_next;
	volatile u32 reserved_threads;

	HANDLE semaphore;

	TaskThreadData thread_data[TASK_THREAD_MAX];
	u32 thread_count;

	b8 running;

} TaskSystemData;

HANDLE console_handle;

typedef struct
{
	LARGE_INTEGER clock_frequency;
	LARGE_INTEGER begin_time;
	volatile f64 last_time;
	f64 add_time;

	HINSTANCE hinstance;
	HINSTANCE user_lib;
	HWND handle;

	v2 mouse_position;
	b8 close_request;
	b8 resize;
	b8 show_cursor;
	v2_i32 last_raw_mouse_dragging;
	v2_i32 raw_mouse_dragging;

	b8 has_focus;

	b8 in_fullscreen;
	u8 fullscreen_request;
	LONG style_before_fullscreen;
	v4_i32 pos_before_fullscreen;

	TaskSystemData task_system;

} WindowsData;

static WindowsData *windows = NULL;

void filepath_resolve(char *dst, const char *src, FilepathType type)
{
	if (path_is_absolute(src))
		string_copy(dst, src, FILE_PATH_SIZE);
	else
	{
		string_copy(dst, (type == FilepathType_Asset) ? "assets/" : "", FILE_PATH_SIZE);
		string_append(dst, src, FILE_PATH_SIZE);
	}
}

void filepath_user(char *dst)
{
	SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, dst);
	while (*dst != '\0')
	{
		if (*dst == '\\')
			*dst = '/';
		++dst;
	}
}

inline Key wparam_to_key(WPARAM wParam)
{
	Key key;

	if (wParam >= 'A' && wParam <= 'Z')
	{
		key = (Key)((u32)Key_A + (u32)wParam - 'A');
	}
	else if (wParam >= '0' && wParam <= '9')
	{

		key = (Key)((u32)Key_Num0 + (u32)wParam - '0');
	}
	else if (wParam >= VK_F1 && wParam <= VK_F24)
	{
		key = (Key)((u32)Key_F1 + (u32)wParam - VK_F1);
	}
	else
	{
		switch (wParam)
		{

		case VK_INSERT:
			key = Key_Insert;
			break;

		case VK_SPACE:
			key = Key_Space;
			break;

		case VK_SHIFT:
			key = Key_Shift;
			break;

		case VK_CONTROL:
			key = Key_Control;
			break;

		case VK_ESCAPE:
			key = Key_Escape;
			break;

		case VK_LEFT:
			key = Key_Left;
			break;

		case VK_RIGHT:
			key = Key_Right;
			break;

		case VK_UP:
			key = Key_Up;
			break;

		case VK_DOWN:
			key = Key_Down;
			break;

		case 13:
			key = Key_Enter;
			break;

		case 8:
			key = Key_Delete;
			break;

		case 46:
			key = Key_Supr;
			break;

		case VK_TAB:
			key = Key_Tab;
			break;

		case VK_CAPITAL:
			key = Key_Capital;
			break;

		case VK_MENU:
			key = Key_Alt;
			break;

		case 36:
			key = Key_Begin;
			break;

		case 35:
			key = Key_End;
			break;

		default:
			// TODO SV_LOG_WARNING("Unknown keycode: %u", (u32)wParam);
			key = Key_None;
			break;
		}
	}

	return key;
}

inline i32 key_to_winkey(Key key)
{
	i32 res;

	if (key >= Key_A && key <= Key_Z)
	{
		res = 'A' + (key - Key_A);
	}
	else if (key >= Key_Num0 && key <= Key_Num9)
	{
		res = '0' + (key - Key_Num0);
	}
	else if (key >= Key_F1 && key <= Key_F24)
	{
		res = VK_F1 + (key - Key_F1);
	}
	else
	{
		switch (key)
		{

		case Key_Insert:
			res = VK_INSERT;
			break;

		case Key_Space:
			res = VK_SPACE;
			break;

		case Key_Shift:
			res = VK_SHIFT;
			break;

		case Key_Control:
			res = VK_CONTROL;
			break;

		case Key_Escape:
			res = VK_ESCAPE;
			break;

		case Key_Left:
			res = VK_LEFT;
			break;

		case Key_Right:
			res = VK_RIGHT;
			break;

		case Key_Up:
			res = VK_UP;
			break;

		case Key_Down:
			res = VK_DOWN;
			break;

		case Key_Enter:
			res = 13;
			break;

		case Key_Delete:
			res = 8;
			break;

		case Key_Supr:
			res = 46;
			break;

		case Key_Tab:
			res = VK_TAB;
			break;

		case Key_Capital:
			res = VK_CAPITAL;
			break;

		case Key_Alt:
			res = VK_MENU;
			break;

		case Key_Begin:
			res = 36;
			break;

		case Key_End:
			res = 35;
			break;

		default:
			// TODO SV_LOG_WARNING("Unknown keycode: %u", (u32)wParam);
			res = 0;
			break;
		}
	}

	return res;
}

inline i32 mouse_button_to_winkey(MouseButton button)
{
	switch (button)
	{
	case MouseButton_Center:
		return VK_MBUTTON;

	case MouseButton_Left:
		return VK_LBUTTON;

	case MouseButton_Right:
		return VK_RBUTTON;

	default:
		return 0;
	}
}

inline v2_u32 get_window_size()
{
	RECT rect;
	if (GetClientRect(windows->handle, &rect))
	{

		v2_u32 size;
		size.x = rect.right - rect.left;
		size.y = rect.bottom - rect.top;
		return size;
	}

	return (v2_u32){0, 0};
}

LRESULT CALLBACK window_proc(
	_In_ HWND wnd,
	_In_ UINT msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam)
{
	LRESULT result = 0;

	switch (msg)
	{

	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	break;

	case WM_CLOSE:
	{
		windows->close_request = TRUE;
		return 0;
	}
	break;

	case WM_SETFOCUS:
	{
		windows->has_focus = TRUE;
	}
	break;

	case WM_KILLFOCUS:
	{
		windows->has_focus = FALSE;
	}
	break;

	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		Key key = wparam_to_key(wParam);

		if (~lParam & (1 << 30))
		{

			if (key != Key_None)
			{

				_input_key_set_pressed(key);
			}
		}

		switch (key)
		{

		case Key_Begin:
			_input_text_command_add(TextCommand_Begin);
			break;

		case Key_End:
			_input_text_command_add(TextCommand_End);
			break;

		case Key_Left:
			_input_text_command_add(TextCommand_Left);
			break;

		case Key_Right:
			_input_text_command_add(TextCommand_Right);
			break;

		case Key_Up:
			_input_text_command_add(TextCommand_Up);
			break;

		case Key_Down:
			_input_text_command_add(TextCommand_Down);
			break;
		}

		break;
	}
	case WM_SYSKEYUP:
	case WM_KEYUP:
	{
		Key key = wparam_to_key(wParam);

		if (key != Key_None)
		{

			_input_key_set_released(key);
		}

		break;
	}
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		MouseButton button = MouseButton_Left;

		switch (msg)
		{
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			button = MouseButton_Right;
			break;

		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			button = MouseButton_Center;
			break;
		}

		switch (msg)
		{
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			_input_mouse_button_set_pressed(button);
			break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
			_input_mouse_button_set_released(button);
			break;
		}
	}
	break;

	case WM_MOUSEMOVE:
	{
		u16 _x = LOWORD(lParam);
		u16 _y = HIWORD(lParam);

		v2_u32 size = get_window_size();
		f32 w = (f32)size.x;
		f32 h = (f32)size.y;

		windows->mouse_position.x = ((f32)_x / w) - 0.5f;
		windows->mouse_position.y = -((f32)_y / h) + 0.5f;

		break;
	}
	case WM_CHAR:
	{
		// SV_LOG_INFO("%u\n", wParam);

		switch (wParam)
		{
		case 0x08:
		case 127:
			_input_text_command_add(TextCommand_DeleteLeft);
			break;

		case 0x0A:

			// Process a linefeed.

			break;

		case 0x1B:
			_input_text_command_add(TextCommand_Escape);
			break;

		case 0x0D:
			_input_text_command_add(TextCommand_Enter);
			break;

		case 3:
			_input_text_command_add(TextCommand_Copy);
			break;

		case 22:
			_input_text_command_add(TextCommand_Paste);
			break;

		case 24:
			_input_text_command_add(TextCommand_Cut);
			break;

		default:
		{
			char c = (char)wParam;

			if (c == 9 || (c >= 32 && c <= 126))
				_input_text_add(c);
		}
		break;
		}

		break;
	}

	case WM_INPUT:
	{
		UINT size;
		u8 buffer[1000];

		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER)) == -1)
		{
			SV_LOG_ERROR("Can't recive raw input data\n");
			break;
		}

		if (size > 1000)
		{
			SV_LOG_ERROR("Raw input data is too large\n");
			break;
		}

		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) != size)
		{
			SV_LOG_ERROR("Can't recive raw input data\n");
			break;
		}

		const RAWINPUT *input = (const RAWINPUT *)buffer;
		if (input->header.dwType == RIM_TYPEMOUSE && (input->data.mouse.lLastX != 0 || input->data.mouse.lLastY != 0))
		{
			windows->raw_mouse_dragging.x += input->data.mouse.lLastX;
			windows->raw_mouse_dragging.y -= input->data.mouse.lLastY;
		}
	}
	break;

	case WM_SIZE:
	{
		/*switch (wParam)
		{
		case SIZE_MAXIMIZED:
			windows->state = WindowState_Maximized;
			break;
		case SIZE_MINIMIZED:
			windows->state = WindowState_Minimized;
			break;
			}*/

		windows->resize = TRUE;

		break;
	}
	case WM_MOUSEWHEEL:
	{
		_input_mouse_wheel_set((f32)(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
		break;
	}

	default:
	{
		result = DefWindowProc(wnd, msg, wParam, lParam);
	}
	break;
	}

	return result;
}

inline void configure_thread(HANDLE thread, const char *name, u64 affinity_mask, i32 priority)
{
	// Put the thread in a dedicated hardware core
	{
		DWORD_PTR mask = affinity_mask;
		DWORD_PTR res = SetThreadAffinityMask(thread, mask);
		assert(res > 0);
	}

	// Set thread priority
	{
		BOOL res = SetThreadPriority(thread, priority);
		assert(res != 0);
	}

#if SV_SLOW
	// Set thread name
	{
		HRESULT hr = SetThreadDescription(thread, (PCWSTR)name);
		assert(SUCCEEDED(hr));
	}
#endif
}

b8 os_initialize(const PlatformInitializeDesc *desc)
{
	windows = memory_allocate(sizeof(WindowsData));

	SetConsoleOutputCP(CP_UTF8);

	if (!QueryPerformanceFrequency(&windows->clock_frequency))
	{
		SV_LOG_ERROR("Can't get the clock frequency\n");
		return FALSE;
	}

	QueryPerformanceCounter(&windows->begin_time);
	windows->add_time = 0.0;
	windows->last_time = 0.0;

	configure_thread(GetCurrentThread(), "main_thread", 1ULL, THREAD_PRIORITY_HIGHEST);

	windows->hinstance = GetModuleHandle(NULL);
	console_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	{
		WNDCLASSA c;
		memory_zero(&c, sizeof(c));

		c.hCursor = LoadCursorA(0, IDC_ARROW);
		c.lpfnWndProc = window_proc;
		c.lpszClassName = "SilverWindow";
		c.hInstance = windows->hinstance;

		if (!RegisterClassA(&c))
		{
			SV_LOG_ERROR("Can't register window class");
			return 1;
		}
	}

	windows->handle = 0;

	const char *title = string_validate(desc->window.title);
	v2_u32 size = desc->window.size;
	v2_u32 pos = desc->window.pos;

	if (title[0] == '\0')
		title = "Untitled";

	u32 flags = 0u;

	if (desc->window.open)
	{

		flags = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_OVERLAPPED | WS_BORDER | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX;
	}

	windows->handle = CreateWindowExA(0u,
									   "SilverWindow",
									   title,
									   flags,
									   pos.x, pos.y, size.x, size.y,
									   0, 0, 0, 0);

	if (windows->handle == 0)
	{
		return FALSE;
	}

	windows->show_cursor = TRUE;

	SV_CHECK(_task_initialize());

	// Register raw input
	{
		RAWINPUTDEVICE device;
		device.usUsagePage = 0x01;
		device.usUsage = 0x02;
		device.dwFlags = 0;
		device.hwndTarget = NULL;

		if (!RegisterRawInputDevices(&device, 1, sizeof(device)))
		{

			SV_LOG_ERROR("Can't register raw input devices\n");
			return FALSE;
		}
	}

	return TRUE;
}

b8 input_focus()
{
	return windows->has_focus;
}

b8 platform_recive_input()
{
	/*if (windows->resize_request) {

		windows->resize_request = FALSE;
		SetWindowLongPtrW(windows->handle, GWL_STYLE, (LONG_PTR)windows->new_style);
		SetWindowPos(windows->handle, 0, windows->new_position.x, windows->new_position.y, windows->new_size.x, windows->new_size.y, 0);
		}*/

	windows->last_raw_mouse_dragging = windows->raw_mouse_dragging;

	if (windows->fullscreen_request)
	{
		if (windows->fullscreen_request == 1 && windows->in_fullscreen)
		{
			LONG style = windows->style_before_fullscreen;
			v4_i32 pos = windows->pos_before_fullscreen;

			SetWindowLongA(windows->handle, GWL_STYLE, style);
			SetWindowPos(windows->handle, HWND_TOP, pos.x, pos.y, pos.z, pos.w, 0);

			SV_LOG_INFO("Fullscreen off\n");
			windows->in_fullscreen = FALSE;
		}
		else if (windows->fullscreen_request == 2 && !windows->in_fullscreen)
		{
			// Save last state
			{
				windows->style_before_fullscreen = GetWindowLongA(windows->handle, GWL_STYLE);
				RECT rect;

				if (GetWindowRect(windows->handle, &rect))
				{
					windows->pos_before_fullscreen.x = rect.left;
					windows->pos_before_fullscreen.y = rect.top;
					windows->pos_before_fullscreen.z = rect.right - rect.left;
					windows->pos_before_fullscreen.w = rect.bottom - rect.top;
				}
			}

			LONG style = WS_POPUP | WS_VISIBLE;
			v2_u32 size = desktop_size();

			SetWindowLongA(windows->handle, GWL_STYLE, style);
			SetWindowPos(windows->handle, HWND_TOP, 0, 0, size.x, size.y, 0);

			SV_LOG_INFO("Fullscreen on\n");
			windows->in_fullscreen = TRUE;
		}

		windows->fullscreen_request = 0;
	}

	MSG msg;

	while (PeekMessageA(&msg, 0, 0u, 0u, PM_REMOVE) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	// Read xinput
	{
		foreach(i, XUSER_MAX_COUNT)
		{
			XINPUT_STATE state;
			if (XInputGetState(i, &state) == ERROR_SUCCESS)
			{
				XINPUT_GAMEPAD* gamepad = &state.Gamepad;
				
				if (gamepad->wButtons & XINPUT_GAMEPAD_X)
					_input_gamepad_press(GamepadButton_X);
				if (gamepad->wButtons & XINPUT_GAMEPAD_B)
					_input_gamepad_press(GamepadButton_B);
				if (gamepad->wButtons & XINPUT_GAMEPAD_A)
					_input_gamepad_press(GamepadButton_A);
				if (gamepad->wButtons & XINPUT_GAMEPAD_Y)
					_input_gamepad_press(GamepadButton_Y);
				
				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
					_input_gamepad_press(GamepadButton_Up);
				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					_input_gamepad_press(GamepadButton_Down);
				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
					_input_gamepad_press(GamepadButton_Left);
				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
					_input_gamepad_press(GamepadButton_Right);

				if (gamepad->wButtons & XINPUT_GAMEPAD_START)
					_input_gamepad_press(GamepadButton_Start);
				if (gamepad->wButtons & XINPUT_GAMEPAD_BACK)
					_input_gamepad_press(GamepadButton_Back);

				if (gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
				{
					// TODO: SV_LOG_INFO("XINPUT_GAMEPAD_LEFT_SHOULDER\n");
				}
				if (gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
				{
					// TODO: SV_LOG_INFO("XINPUT_GAMEPAD_RIGHT_SHOULDER\n");
				}
				
				u32 left_stickX = (i32)gamepad->sThumbLX + 32768;
				u32 left_stickY = (i32)gamepad->sThumbLY + 32768;

				u32 right_stickX = (i32)gamepad->sThumbRX + 32768;
				u32 right_stickY = (i32)gamepad->sThumbRY + 32768;

				f32 div = (f32)(32768 + 32767);

				v2 left;
				left.x = (f32)left_stickX / div;
				left.y = (f32)left_stickY / div;
				left = v2_mul_scalar(v2_sub_scalar(left, 0.5f), 2.f);

				v2 right;
				right.x = (f32)right_stickX / div;
				right.y = (f32)right_stickY / div;
				right = v2_mul_scalar(v2_sub_scalar(right, 0.5f), 2.f);

				_input_gamepad_set_joystick(TRUE, left);
				_input_gamepad_set_joystick(FALSE, right);
			}
		}
	}

	// Send mouse info
	{
		RECT r;
		GetClientRect(windows->handle, &r);

		f32 width = r.right - r.left;
		f32 height = r.bottom - r.top;

		v2 drag;
		drag.x = (windows->raw_mouse_dragging.x - windows->last_raw_mouse_dragging.x) / width;
		drag.y = (windows->raw_mouse_dragging.y - windows->last_raw_mouse_dragging.y) / height;

		_input_mouse_position_set(windows->mouse_position);
		_input_mouse_dragging_set(drag);
	}

	if (windows->resize)
	{
		windows->resize = FALSE;

#if SV_GRAPHICS
		graphics_swapchain_resize();
#endif
	}

	// Mouse clipping
	{
		if (!windows->show_cursor && windows->has_focus)
		{
			RECT rect;
			if (GetWindowRect(windows->handle, &rect))
			{

				i32 width = rect.right - rect.left;
				i32 height = rect.bottom - rect.top;

				rect.left = rect.left + width / 2 - 1;
				rect.right = rect.right - width / 2 + 1;
				rect.top = rect.top + height / 2 - 1;
				rect.bottom = rect.bottom - height / 2 + 1;

				ClipCursor(&rect);
			}
		}
		else
		{
			ClipCursor(NULL);
		}
	}

	if (input_key(Key_Alt, InputState_Any) && input_key(Key_F4, InputState_Pressed))
	{
		windows->close_request = TRUE;
	}
	if (input_key(Key_F11, InputState_Released))
	{

		WindowState state = window_state();

		if (state == WindowState_Fullscreen)
		{
			SV_LOG_INFO("State: Fullscreen\n");
		}
		else if (state == WindowState_Maximized)
		{
			SV_LOG_INFO("State: Maximized\n");
		}
		else
		{
			SV_LOG_INFO("State: Normal\n");
		}

		set_window_fullscreen(state != WindowState_Fullscreen);
	}

	return !windows->close_request;
}

void os_close()
{
	if (windows)
	{
		_task_close();

		if (windows->handle)
			DestroyWindow(windows->handle);

		if (windows->user_lib)
			FreeLibrary(windows->user_lib);

		memory_free(windows);
	}
}

void windows_print(PrintStyle style, const char *str, ...)
{
	va_list args;
	va_start(args, str);

	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	WORD saved_attributes;

	// Save current attributes
	GetConsoleScreenBufferInfo(console_handle, &consoleInfo);
	saved_attributes = consoleInfo.wAttributes;

	i32 att;

	switch (style)
	{

	case PrintStyle_Warning:
		att = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;

	case PrintStyle_Error:
		att = FOREGROUND_RED | FOREGROUND_INTENSITY;
		break;

	default:
		att = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	}

	SetConsoleTextAttribute(console_handle, att);
	vprintf(str, args);

	// Restore attributes
	SetConsoleTextAttribute(console_handle, saved_attributes);

	va_end(args);
}

void show_message(const char *title, const char *content, b8 error)
{
	MessageBox(0, content, title, MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

b8 show_dialog_yesno(const char *title, const char *content)
{
	int res = MessageBox(0, content, title, MB_YESNO | MB_ICONQUESTION);

	return res == IDYES;
}

// Window

u64 window_handle()
{
	return (u64)windows->handle;
}

v2_u32 window_size()
{
	return get_window_size();
}

WindowState window_state()
{
	LONG style = GetWindowLongA(windows->handle, GWL_STYLE);

	if (style & WS_POPUP)
		return WindowState_Fullscreen;
	if (style & WS_MAXIMIZE)
		return WindowState_Maximized;
	if (style & WS_MINIMIZE)
		return WindowState_Minimized;

	return WindowState_Windowed;
}

void set_window_fullscreen(b8 fullscreen)
{
	windows->fullscreen_request = fullscreen + 1;
}

v2_u32 desktop_size()
{
	HWND desktop = GetDesktopWindow();
	RECT rect;
	GetWindowRect(desktop, &rect);
	return (v2_u32){.x = (u32)rect.right, .y = (u32)rect.bottom};
}

// Cursor

void cursor_hide()
{
	if (windows->show_cursor)
	{
		ShowCursor(FALSE);
		windows->show_cursor = FALSE;
	}
}

void cursor_show()
{
	if (!windows->show_cursor)
	{
		ShowCursor(TRUE);
		windows->show_cursor = TRUE;
	}
}

// File Management

b8 file_dialog(char *buff, u32 filterCount, const char **filters, const char *filepath, b8 open, b8 is_file)
{
	buff[0] = '\0';

	char abs_filter[5000] = "";

	{
		char *it = abs_filter;

		for (u32 i = 0; i < filterCount; ++i)
		{
			string_copy(it, filters[i * 2u], SV_ARRAY_SIZE(abs_filter));
			it += string_size(it) + 1;

			string_copy(it, filters[i * 2u + 1], SV_ARRAY_SIZE(abs_filter));
			it += string_size(it) + 1;
		}
		it[0] = '\0';
		it[1] = '\0';
	}

	OPENFILENAMEA file;
	SV_ZERO(file);

	file.lStructSize = sizeof(OPENFILENAMEA);
	file.hwndOwner = windows->handle;
	file.lpstrFilter = abs_filter;
	file.nFilterIndex = 1u;
	file.lpstrFile = buff;
	file.lpstrInitialDir = filepath;
	file.nMaxFile = MAX_PATH;
	file.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

	b8 result;

	if (open)
		result = GetOpenFileNameA(&file);
	else
		result = GetSaveFileNameA(&file);

	if (result == TRUE)
	{
		path_clear(buff);
		return TRUE;
	}
	else
	{
		DWORD err = CommDlgExtendedError();
		return FALSE;
	}
}

b8 file_dialog_open(char *buff, u32 filterCount, const char **filters, const char *startPath)
{
	return file_dialog(buff, filterCount, filters, startPath, TRUE, TRUE);
}

b8 file_dialog_save(char *buff, u32 filterCount, const char **filters, const char *startPath)
{
	return file_dialog(buff, filterCount, filters, startPath, FALSE, TRUE);
}

void path_clear(char *path)
{
	while (*path != '\0')
	{

		if (*path == '\\')
		{
			*path = '/';
		}

		++path;
	}
}

b8 file_read_binary(FilepathType type, const char *filepath_, u8 **data, u32 *psize)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	HANDLE file = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	DWORD size;
	size = GetFileSize(file, NULL);

	*psize = size;
	*data = memory_allocate(size);

	SetFilePointer(file, 0, NULL, FILE_BEGIN);
	ReadFile(file, *data, size, NULL, NULL);

	CloseHandle(file);
	return TRUE;
}

b8 file_read_text(FilepathType type, const char *filepath_, u8 **data, u32 *psize)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	HANDLE file = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	DWORD size;
	size = GetFileSize(file, NULL);

	*psize = size;
	*data = memory_allocate(size + 1);

	SetFilePointer(file, 0, NULL, FILE_BEGIN);
	ReadFile(file, *data, size, NULL, NULL);
	(*data)[size] = '\0';

	CloseHandle(file);
	return TRUE;
}

inline b8 create_path(const char *filepath)
{
	char folder[FILE_PATH_SIZE] = "\0";

	while (TRUE)
	{

		size_t folder_size = strlen(folder);

		const char *it = filepath + folder_size;
		while (*it && *it != '/')
			++it;

		if (*it == '\0')
			break;
		else
			++it;

		if (*it == '\0')
			break;

		folder_size = it - filepath;
		memory_copy(folder, filepath, folder_size);
		folder[folder_size] = '\0';

		if (folder_size == 3u && folder[1u] == ':')
			continue;

		if (!CreateDirectory(folder, NULL) && ERROR_ALREADY_EXISTS != GetLastError())
		{
			return FALSE;
		}
	}

	return TRUE;
}

b8 file_write_binary(FilepathType type, const char *filepath_, const u8 *data, size_t size, b8 append, b8 recursive)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	HANDLE file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{

		if (recursive)
		{

			if (!create_path(filepath))
				return FALSE;

			file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file == INVALID_HANDLE_VALUE)
				return FALSE;
		}
		else
			return FALSE;
	}

	if (append)
		SetFilePointer(file, 0, NULL, FILE_END);

	WriteFile(file, data, (DWORD)size, NULL, NULL);

	CloseHandle(file);
	return TRUE;
}

b8 file_write_text(FilepathType type, const char *filepath_, const char *str, size_t size, b8 append, b8 recursive)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	HANDLE file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		if (recursive)
		{

			if (!create_path(filepath))
				return FALSE;

			file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file == INVALID_HANDLE_VALUE)
				return FALSE;
		}
		else
			return FALSE;
	}

	if (append)
		SetFilePointer(file, 0, NULL, FILE_END);

	WriteFile(file, str, (DWORD)size, NULL, NULL);

	CloseHandle(file);
	return TRUE;
}

inline Date systemtime_to_date(SYSTEMTIME sys)
{
	Date date;
	date.year = (u32)sys.wYear;
	date.month = (u32)sys.wMonth;
	date.day = (u32)sys.wDay;
	date.hour = (u32)sys.wHour;
	date.minute = (u32)sys.wMinute;
	date.second = (u32)sys.wSecond;
	date.millisecond = (u32)sys.wMilliseconds;

	return date;
}

inline Date filetime_to_date(FILETIME file)
{
	SYSTEMTIME sys;
	FileTimeToSystemTime(&file, &sys);
	return systemtime_to_date(sys);
}

f64 timer_now()
{
	LARGE_INTEGER now, elapsed;
	QueryPerformanceCounter(&now);

	elapsed.QuadPart = now.QuadPart - windows->begin_time.QuadPart;
	elapsed.QuadPart *= 1000000000;
	elapsed.QuadPart /= windows->clock_frequency.QuadPart;

	f64 time = (f64)(elapsed.QuadPart) / 1000000000.0;

	if (time < 0.0)
	{
		WRITE_BARRIER;
		windows->add_time = windows->last_time;
		QueryPerformanceCounter(&windows->begin_time);
		return timer_now();
	}

	time += windows->add_time;

	WRITE_BARRIER;
	windows->last_time = time;
	return time;
}

u64 timer_seed()
{
	LARGE_INTEGER now, elapsed;
	QueryPerformanceCounter(&now);

	elapsed.QuadPart = now.QuadPart - windows->begin_time.QuadPart;
	elapsed.QuadPart *= 1000000000;
	elapsed.QuadPart /= windows->clock_frequency.QuadPart;

	return hash_combine(elapsed.QuadPart * 0x92847ULL + 0x82744ULL, 0x7677634345ULL);
}

Date timer_date()
{
	SYSTEMTIME time;
	GetLocalTime(&time);
	return systemtime_to_date(time);
}

b8 file_date(FilepathType type, const char *filepath_, Date *create, Date *last_write, Date *last_access)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	HANDLE file = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;

	FILETIME creation_time;
	FILETIME last_access_time;
	FILETIME last_write_time;

	if (GetFileTime(file, &creation_time, &last_access_time, &last_write_time))
	{

		if (create)
			*create = filetime_to_date(creation_time);
		if (last_access)
			*last_access = filetime_to_date(last_access_time);
		if (last_write)
			*last_write = filetime_to_date(last_write_time);
	}
	else
	{
		CloseHandle(file);
		return FALSE;
	}

	CloseHandle(file);
	return TRUE;
}

b8 file_remove(FilepathType type, const char *filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	return DeleteFile(filepath);
}

b8 file_copy(FilepathType type, const char *srcpath_, const char *dstpath_)
{
	char srcpath[MAX_PATH];
	char dstpath[MAX_PATH];
	filepath_resolve(srcpath, srcpath_, type);
	filepath_resolve(dstpath, dstpath_, type);

	if (!CopyFileA(srcpath, dstpath, FALSE))
	{
		create_path(dstpath);
		return CopyFileA(srcpath, dstpath, FALSE);
	}

	return TRUE;
}

b8 file_exists(FilepathType type, const char *filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	DWORD att = GetFileAttributes(filepath);
	if (INVALID_FILE_ATTRIBUTES == att)
	{
		return FALSE;
	}
	return TRUE;
}

b8 folder_create(FilepathType type, const char *filepath_, b8 recursive)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	if (recursive)
		create_path(filepath);
	return CreateDirectory(filepath, NULL);
}

b8 folder_remove(FilepathType type, const char *filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);

	return RemoveDirectoryA(filepath);
}

inline FolderElement finddata_to_folderelement(WIN32_FIND_DATAA d)
{
	FolderElement e;
	e.is_file = !(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	e.create_date = filetime_to_date(d.ftCreationTime);
	e.last_write_date = filetime_to_date(d.ftLastWriteTime);
	e.last_access_date = filetime_to_date(d.ftLastAccessTime);
	sprintf(e.name, "%s", (const char *)d.cFileName);
	size_t size = strlen(e.name);
	if (size == 0u)
		e.extension = NULL;
	else
	{
		const char *begin = e.name;
		const char *it = begin + size;

		while (it != begin && *it != '.')
		{
			--it;
		}

		if (it == begin)
		{
			e.extension = NULL;
		}
		else
		{
			assert(*it == '.');
			e.extension = it + 1u;
		}
	}
	return e;
}

FolderIterator folder_iterator_begin(FilepathType type, const char *folderpath__)
{
	WIN32_FIND_DATAA data;

	char folderpath_[MAX_PATH];
	filepath_resolve(folderpath_, folderpath__, type);

	// Clear path
	char folderpath[MAX_PATH];

	const char *it = folderpath_;
	char *it0 = folderpath;

	if (!path_is_absolute(folderpath_))
	{
		*it0++ = '.';
		*it0++ = '\\';
	}

	while (*it != '\0')
	{
		if (*it == '/')
			*it0 = '\\';
		else
			*it0 = *it;

		++it;
		++it0;
	}

	if (*(it0 - 1u) != '\\')
		*it0++ = '\\';
	*it0++ = '*';
	*it0++ = '\0';

	FolderIterator iterator;
	iterator.has_next = FALSE;
	HANDLE find = FindFirstFileA(folderpath, &data);

	if (find == INVALID_HANDLE_VALUE)
		return iterator;

	iterator.has_next = TRUE;
	iterator.element = finddata_to_folderelement(data);
	iterator._handle = (u64)find;

	return iterator;
}

void folder_iterator_next(FolderIterator *it)
{
	HANDLE find = (HANDLE)it->_handle;

	WIN32_FIND_DATAA data;
	if (FindNextFileA(find, &data))
	{

		it->element = finddata_to_folderelement(data);
	}
	else
	{
		it->has_next = FALSE;
		FindClose(find);
	}
}

void folder_iterator_close(FolderIterator *it)
{
	HANDLE find = (HANDLE)it->_handle;
	if (it->has_next)
		FindClose(find);
}

//////////////////////////////// CLIPBOARD ////////////////////////////

b8 clipboard_write_ansi(const char *text)
{
	SV_CHECK(OpenClipboard(windows->handle));

	EmptyClipboard();

	u32 size = string_size(text);

	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size + 1);
	memory_copy(GlobalLock(mem), text, size + 1);
	GlobalUnlock(mem);

	HANDLE res = SetClipboardData(CF_TEXT, mem);
	CloseClipboard();

	return res != NULL;
}

const char *clipboard_read_ansi()
{
	if (!OpenClipboard(windows->handle))
		return NULL;

	const char *txt = (const char *)GetClipboardData(CF_TEXT);

	CloseClipboard();

	return txt;
}

//////////////////////////////// MULTITHREADING ////////////////////////////

Mutex mutex_create()
{
	HANDLE handle = (u64)CreateMutexA(NULL, FALSE, NULL);
	return (Mutex)handle;
}

void mutex_destroy(Mutex mutex)
{
	if (mutex)
	{
		CloseHandle((HANDLE)mutex);
	}
}

void mutex_lock(Mutex mutex)
{
	if (mutex)
	{
		WaitForSingleObject((HANDLE)mutex, INFINITE);
	}

	assert_title(mutex, "The mutex must be valid");
}

b8 mutex_try_lock(Mutex mutex)
{
	b8 lock = FALSE;

	if (mutex)
	{
		lock = WaitForSingleObject((HANDLE)mutex, 0) != WAIT_TIMEOUT;
	}

	assert_title(mutex, "The mutex must be valid");
	return lock;
}

void mutex_unlock(Mutex mutex)
{
	if (mutex)
	{
		ReleaseMutex((HANDLE)mutex);
	}

	assert_title(mutex, "The mutex must be valid");
}

Thread thread_create(ThreadMainFn main, void *data)
{
	HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, data, 0, NULL);
	return (Thread)handle;
}

void thread_destroy(Thread thread)
{
	if (thread)
	{

		TerminateThread((HANDLE)thread, 0);
	}
}

void thread_wait(Thread thread)
{
	if (thread)
	{

		WaitForSingleObject((HANDLE)thread, INFINITE);
	}

	assert_title(thread, "The thread must be valid");
}

void thread_sleep(u64 millis)
{
	Sleep(millis);
}

void thread_yield()
{
	SwitchToThread();
}

u64 thread_id()
{
	return (u64)GetCurrentThreadId();
}

void thread_configure(Thread thread, const char *name, u64 affinity_mask, ThreadPrority priority)
{
	u32 win_priority;

	switch (priority)
	{

	case ThreadPrority_Highest:
		win_priority = THREAD_PRIORITY_HIGHEST;
		break;

	case ThreadPrority_High:
		win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
		break;

	case ThreadPrority_Normal:
		win_priority = THREAD_PRIORITY_NORMAL;
	default:

	case ThreadPrority_Low:
		win_priority = THREAD_PRIORITY_BELOW_NORMAL;
		break;

	case ThreadPrority_Lowest:
		win_priority = THREAD_PRIORITY_LOWEST;
		break;
	}

	configure_thread((HANDLE)thread, name, affinity_mask, win_priority);
}

static DWORD WINAPI task_thread(void *arg);

static b8 _task_initialize()
{
	TaskSystemData *data = &windows->task_system;
	data->running = TRUE;

	u32 thread_count;

	// Compute preferred thread count
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);

		thread_count = SV_MAX(sysinfo.dwNumberOfProcessors, 1);
	}

	u64 affinity_offset = 1;

	if (thread_count == 1)
	{
		affinity_offset = 0;
	}
	else
		thread_count--;

	thread_count = SV_MIN(thread_count, TASK_THREAD_MAX);

	data->semaphore = CreateSemaphoreExA(NULL, 0, thread_count, NULL, 0, SEMAPHORE_ALL_ACCESS);

	if (data->semaphore == NULL)
		return FALSE;

	foreach (t, thread_count)
	{

		TaskThreadData *thread_data = data->thread_data + t;
		thread_data->id = t;

		thread_data->thread = CreateThread(NULL, 0, task_thread, thread_data, 0, NULL);

		if (thread_data->thread == NULL)
		{
			SV_LOG_ERROR("Can't create task thread\n");
			data->running = FALSE;
			return FALSE;
		}

		// Config thread
		{
			char name[200];
			string_copy(name, "task_", 200);

			char id_str[30];
			string_from_u32(id_str, t);

			string_append(name, id_str, 200);

			configure_thread(thread_data->thread, name, 1ull << (affinity_offset + (u64)t), THREAD_PRIORITY_HIGHEST);
		}
	}

	data->thread_count = thread_count;

	return TRUE;
}

static void _task_close()
{
	task_join();

	TaskSystemData *data = &windows->task_system;
	data->running = FALSE;

	HANDLE threads[TASK_THREAD_MAX];
	foreach (i, data->thread_count)
		threads[i] = data->thread_data[i].thread;

	ReleaseSemaphore(data->semaphore, data->thread_count, 0);

	if (WaitForMultipleObjectsEx(data->thread_count, threads, TRUE, 1000, FALSE) >= data->thread_count)
	{
		SV_LOG_ERROR("Can't close task threads properly\n");
	}

	CloseHandle(data->semaphore);
}

inline b8 _task_thread_do_work()
{
	TaskSystemData *data = &windows->task_system;
	b8 done = FALSE;

	u32 task_next = data->task_next;

	if (task_next < data->task_count)
	{
		u32 task_index = InterlockedCompareExchange((volatile LONG *)&data->task_next, task_next + 1, task_next);
		READ_BARRIER;

		if (task_index == task_next)
		{
			TaskData task = data->tasks[task_index % TASK_QUEUE_SIZE];

			// Task function
			if (task.type == 1)
			{
				assert(task.fn != NULL);

				TaskFn fn = task.fn;
				fn(task.user_data);

				InterlockedIncrement((volatile LONG *)&data->task_completed);
				if (task.context != NULL)
					InterlockedIncrement((volatile LONG *)&task.context->completed);
				done = TRUE;
			}
			// Reserve thread
			else if (task.type == 2)
			{
				InterlockedIncrement((volatile LONG *)&data->task_completed);
				InterlockedIncrement((volatile LONG *)&data->reserved_threads);

				ThreadMainFn fn = task.fn;
				void *main_data = *(void **)task.user_data;

				fn(main_data);

				if (task.context != NULL)
					InterlockedIncrement((volatile LONG *)&task.context->completed);
				InterlockedDecrement((volatile LONG *)&data->reserved_threads);
			}
		}
	}

	return done;
}

static DWORD WINAPI task_thread(void *arg)
{
	TaskThreadData *thread = arg;
	TaskSystemData *data = &windows->task_system;

	while (data->running)
	{
		if (!_task_thread_do_work() && !task_running(NULL))
		{
			WaitForSingleObjectEx(data->semaphore, INFINITE, FALSE);
		}
	}

	return 0;
}

static void _task_add_queue(TaskDesc desc, TaskContext *ctx)
{
	assert_title(desc.fn != NULL, "Null task function");
	assert_title(desc.size <= TASK_DATA_SIZE, "The task data size is too large");

	TaskSystemData *data = &windows->task_system;

	TaskData *task = data->tasks + data->task_count % TASK_QUEUE_SIZE;
	task->fn = desc.fn;
	task->context = ctx;
	if (desc.data)
		memory_copy(task->user_data, desc.data, desc.size);
	task->type = 1;

	WRITE_BARRIER;
	++data->task_count;

	ReleaseSemaphore(data->semaphore, 1, 0);
}

void task_dispatch(TaskDesc *tasks, u32 task_count, TaskContext *context)
{
	TaskSystemData *data = &windows->task_system;

	if (context)
	{
		context->dispatched += task_count;
	}

	foreach (i, task_count)
	{
		_task_add_queue(tasks[i], context);
	}
}

void task_reserve_thread(ThreadMainFn main_fn, void *main_data, TaskContext *context)
{
	TaskSystemData *data = &windows->task_system;

	if (context != NULL)
		context->dispatched++;

	TaskData *task = data->tasks + data->task_count % TASK_QUEUE_SIZE;
	task->fn = main_fn;
	task->context = context;
	memory_copy(task->user_data, &main_data, sizeof(void *));
	task->type = 2;

	WRITE_BARRIER;
	volatile u32 task_index = data->task_count++;

	ReleaseSemaphore(data->semaphore, 1, 0);

	while (task_index >= data->task_next)
		SwitchToThread();
}

void task_join()
{
	TaskSystemData *data = &windows->task_system;

	task_wait(NULL);

	while (data->reserved_threads)
	{
		Sleep(10);
	}
}

void task_wait(TaskContext *context)
{
	TaskSystemData *data = &windows->task_system;

	while (task_running(context))
		_task_thread_do_work();
}

b8 task_running(TaskContext *context)
{
	if (context)
	{
		return context->completed < context->dispatched;
	}
	else
	{
		TaskSystemData *data = &windows->task_system;
		return data->task_completed < data->task_count;
	}
}

u32 interlock_increment_u32(volatile u32 *n)
{
	return InterlockedIncrement((volatile LONG *)n);
}

u32 interlock_decrement_u32(volatile u32 *n)
{
	return InterlockedDecrement((volatile LONG *)n);
}

// DYNAMIC LIBRARIES

Library library_load(FilepathType type, const char *filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_, type);
	return (Library)LoadLibrary(filepath);
}

void library_free(Library library)
{
	FreeLibrary((HINSTANCE)library);
}

void *library_address(Library library, const char *name)
{
	HINSTANCE instance = (HINSTANCE)library;

	if (instance == 0)
		instance = GetModuleHandle(NULL);

	return GetProcAddress(instance, name);
}

// MAIN

#if SV_SLOW
int main()
#else
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#endif
{
	if (!initialize())
		return 1;

	while (hosebase_frame_begin())
	{
		update();
		hosebase_frame_end();
	}

	close();

	return 0;
}

#endif