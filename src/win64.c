#define NOMINMAX
#include "windows.h"

#undef near
#undef far

#pragma comment(lib,"user32.lib")

#include "time.h"

#include "Hosebase/os.h"
#include "Hosebase/input.h"

#include "graphics_internal.h"

typedef struct {

	char origin_path[FILE_PATH_SIZE];

	clock_t begin_time;
	
	HINSTANCE   hinstance;
	HINSTANCE   user_lib;
	HWND        handle;
	v2_u32      size;
	v2_u32      position;

	v2     mouse_position;
	b8     close_request;
	b8     resize;
	
} PlatformData;

static PlatformData* platform = NULL;

void filepath_resolve(char* dst, const char* src)
{
	if (src[0] == '$') {
		++src;
		string_copy(dst, src, FILE_PATH_SIZE);
	}
	else {
		if (path_is_absolute(src))
			string_copy(dst, src, FILE_PATH_SIZE);
		else {
			string_copy(dst, platform->origin_path, FILE_PATH_SIZE);
			string_append(dst, src, FILE_PATH_SIZE);
		}
	}
}

inline Key wparam_to_key(WPARAM wParam)
{
	Key key;

	if (wParam >= 'A' && wParam <= 'Z') {

		key = (Key)((u32)Key_A + (u32)wParam - 'A');
	}
	else if (wParam >= '0' && wParam <= '9') {

		key = (Key)((u32)Key_Num0 + (u32)wParam - '0');
	}
	else if (wParam >= VK_F1 && wParam <= VK_F24) {

		key = (Key)((u32)Key_F1 + (u32)wParam - VK_F1);
	}
	else {
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

LRESULT CALLBACK window_proc (
		_In_ HWND   wnd,
		_In_ UINT   msg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam
	)
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
		platform->close_request = TRUE;
		return 0;
	}
	break;
	
	case WM_ACTIVATEAPP:
	{
	}
	break;

	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		Key key = wparam_to_key(wParam);
		
		if (~lParam & (1 << 30)) {

			if (key != Key_None) {

				_input_key_set_pressed(key);
			}
		}

		switch (key) {

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

		if (key != Key_None) {

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

		f32 w = (f32)platform->size.x;
		f32 h = (f32)platform->size.y;

		platform->mouse_position.x = ((f32)_x / w) - 0.5f;
		platform->mouse_position.y = -((f32)_y / h) + 0.5f;

		break;
	}
	case WM_CHAR:
	{
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

		default:
		{
			char c = (char)wParam;

			if (c == 9 || (c >= 32 && c <= 126))
				_input_text_add(&c);
		}
		break;

		}

		break;
	}

	case WM_SIZE:
	{
		platform->size.x = LOWORD(lParam);
		platform->size.y = HIWORD(lParam);

		/*switch (wParam)
		{
		case SIZE_MAXIMIZED:
			platform->state = WindowState_Maximized;
			break;
		case SIZE_MINIMIZED:
			platform->state = WindowState_Minimized;
			break;
			}*/

		platform->resize = TRUE;

		break;
	}
	case WM_MOVE:
	{
		platform->position.x = LOWORD(lParam);
		platform->position.y = HIWORD(lParam);

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

b8 _os_initialize(const OSInitializeDesc* desc)
{
	platform = memory_allocate(sizeof(PlatformData));

	platform->begin_time = clock();
	
	platform->hinstance = GetModuleHandle(NULL);
	
    {
		WNDCLASSA c;
		memory_zero(&c, sizeof(c));
		
		c.hCursor = LoadCursorA(0, IDC_ARROW);
		c.lpfnWndProc = window_proc;
		c.lpszClassName = "SilverWindow";
		c.hInstance = platform->hinstance;
	    
		if (!RegisterClassA(&c)) {
			print("Can't register window class");
			return 1;
		}
    }
	
    platform->handle = 0;
	platform->handle = CreateWindowExA(0u,
									   "SilverWindow",
									   "SilverEngine",
									   //WS_POPUP | WS_VISIBLE,
									   WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_OVERLAPPED | WS_BORDER | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
									   desc->window_pos.x, desc->window_pos.y, desc->window_size.x, desc->window_size.y,
									   0, 0, 0, 0
		);

	if (platform->handle == 0) {
		return FALSE;
	}
	
	return TRUE;
}

b8 _os_recive_input()
{
	/*if (platform->resize_request) {

		platform->resize_request = FALSE;
		SetWindowLongPtrW(platform->handle, GWL_STYLE, (LONG_PTR)platform->new_style);
		SetWindowPos(platform->handle, 0, platform->new_position.x, platform->new_position.y, platform->new_size.x, platform->new_size.y, 0);
		}*/
	
	MSG msg;
	
	while (PeekMessageA(&msg, 0, 0u, 0u, PM_REMOVE) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	_input_mouse_position_set(platform->mouse_position);

	if (platform->resize) {
		platform->resize = FALSE;
		
		graphics_swapchain_resize();
	}

	return !platform->close_request;
}

void _os_close()
{
	if (platform) {
		
		if (platform->handle)
			DestroyWindow(platform->handle);
		
		if (platform->user_lib)
			FreeLibrary(platform->user_lib);

		memory_free(platform);
	}
}

void print(const char* str, ...)
{
	va_list args;
	va_start(args, str);

	vprintf(str, args);
	
	va_end(args);
}

void show_message(const char* title, const char* content, b8 error)
{
	MessageBox(0, content, title, MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

b8 show_dialog_yesno(const char* title, const char* content)
{
	int res = MessageBox(0, content, title, MB_YESNO | MB_ICONQUESTION);

	return res == IDYES;
}

// Window

u64 window_handle()
{
	return (u64)platform->handle;
}

v2_u32 window_size()
{
	return platform->size;
}

f32 window_aspect()
{
	return (f32)(platform->size.x) / (f32)(platform->size.y);
}

v2_u32 desktop_size()
{
	HWND desktop = GetDesktopWindow();
	RECT rect;
	GetWindowRect(desktop, &rect);
	return (v2_u32){ .x = (u32)rect.right, .y = (u32)rect.bottom };
}

// File Management

/*b8 file_dialog(char* buff, u32 filterCount, const char** filters, const char* filepath_, b8 open, b8 is_file)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	// TODO: Not use classes, this should be in c in the future
	Buffer abs_filter;
	
	for (u32 i = 0; i < filterCount; ++i) {
		abs_filter.write_back(filters[i * 2u], strlen(filters[i * 2u]));
		char c = '\0';
		abs_filter.write_back(&c, sizeof(char));

		abs_filter.write_back(filters[i * 2u + 1u], strlen(filters[i * 2u + 1u]));
		abs_filter.write_back(&c, sizeof(char));
	}

	OPENFILENAMEA file;
	SV_ZERO_MEMORY(&file, sizeof(OPENFILENAMEA));

	file.lStructSize = sizeof(OPENFILENAMEA);
	file.hwndOwner = platform->handle;
	file.lpstrFilter = (char*)abs_filter.data();
	file.nFilterIndex = 1u;
	file.lpstrFile = buff;
	file.lpstrInitialDir = filepath;
	file.nMaxFile = MAX_PATH;
	file.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

	B8 result;

	if (open) result = GetOpenFileNameA(&file);
	else result = GetSaveFileNameA(&file);

	if (result == TRUE) {
		path_clear(buff);
		return TRUE;
	}
	else return FALSE;
	}

b8 file_dialog_open(char* buff, u32 filterCount, const char** filters, const char* startPath)
{
	return file_dialog(buff, filterCount, filters, startPath, TRUE, TRUE);
}
    
b8 file_dialog_save(char* buff, u32 filterCount, const char** filters, const char* startPath)
{
	return file_dialog(buff, filterCount, filters, startPath, FALSE, TRUE);
	}*/
    
b8 path_is_absolute(const char* path)
{
	assert(path);
	size_t size = strlen(path);
	if (size < 2u) return FALSE;
	return path[1] == ':';
}
    
void path_clear(char* path)
{
	while (*path != '\0') {

		if (*path == '\\') {
			*path = '/';
		}
	    
		++path;
	}
}

b8 file_read_binary(const char* filepath_, Buffer* data)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	HANDLE file = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	DWORD size;
	size = GetFileSize(file, NULL);

	buffer_resize(data, (u32)size);
	
	SetFilePointer(file, 0, NULL, FILE_BEGIN);
	ReadFile(file, data->data, size, NULL, NULL);
	
	CloseHandle(file);
	return TRUE;
}

b8 file_read_text(const char* filepath_, DynamicString* str)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	HANDLE file = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	DWORD size;
	size = GetFileSize(file, NULL);

	dynamic_string_resize(str, (u32)size);
	
	SetFilePointer(file, 0, NULL, FILE_BEGIN);
	ReadFile(file, str->data, size, NULL, NULL);
	
	CloseHandle(file);
	return TRUE;
}

inline b8 create_path(const char* filepath)
{	
	char folder[FILE_PATH_SIZE] = "\0";
		
	while (TRUE) {

		size_t folder_size = strlen(folder);

		const char* it = filepath + folder_size;
		while (*it && *it != '/') ++it;

		if (*it == '\0') break;
		else ++it;

		if (*it == '\0') break;

		folder_size = it - filepath;
		memory_copy(folder, filepath, folder_size);
		folder[folder_size] = '\0';

		if (folder_size == 3u && folder[1u] == ':')
			continue;

		if (!CreateDirectory(folder, NULL) && ERROR_ALREADY_EXISTS != GetLastError()) {
			return FALSE;
		}
	}

	return TRUE;
}
    
b8 file_write_binary(const char* filepath_, const u8* data, size_t size, b8 append, b8 recursive)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	HANDLE file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (file == INVALID_HANDLE_VALUE) {
	    
		if (recursive) {
		
			if (!create_path(filepath)) return FALSE;

			file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file == INVALID_HANDLE_VALUE) return FALSE;
		}
		else return FALSE;
	}

	if (append)
		SetFilePointer(file, 0, NULL, FILE_END);
		
	WriteFile(file, data, (DWORD)size, NULL, NULL);
	
	CloseHandle(file);
	return TRUE;
}
    
b8 file_write_text(const char* filepath_, const char* str, size_t size, b8 append, b8 recursive)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	HANDLE file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (file == INVALID_HANDLE_VALUE) {
		if (recursive) {
		
			if (!create_path(filepath)) return FALSE;

			file = CreateFile(filepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file == INVALID_HANDLE_VALUE) return FALSE;
		}
		else return FALSE;
	}

	if (append)
		SetFilePointer(file, 0, NULL, FILE_END);
		
	WriteFile(file, str, (DWORD)size, NULL, NULL);
	
	CloseHandle(file);
	return TRUE;
}

inline Date filetime_to_date(FILETIME file)
{
	SYSTEMTIME sys;
	FileTimeToSystemTime(&file, &sys);

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

f64 timer_now()
{
	clock_t time = clock();
	return (time - platform->begin_time) * 0.001;
}

b8 file_date(const char* filepath_, Date* create, Date* last_write, Date* last_access)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	HANDLE file = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;

	FILETIME creation_time;
	FILETIME last_access_time;
	FILETIME last_write_time;
	
	if (GetFileTime(file, &creation_time, &last_access_time, &last_write_time)) {

		if (create) *create = filetime_to_date(creation_time);
		if (last_access) *last_access = filetime_to_date(last_access_time);
		if (last_write) *last_write = filetime_to_date(last_write_time);
	}
	else {
		CloseHandle(file);
		return FALSE;
	}

	CloseHandle(file);
	return TRUE;
}

b8 file_remove(const char* filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	return DeleteFile(filepath);
}
    
b8 file_copy(const char* srcpath_, const char* dstpath_)
{
	char srcpath[MAX_PATH];
	char dstpath[MAX_PATH];
	filepath_resolve(srcpath, srcpath_);
	filepath_resolve(dstpath, dstpath_);
	
	if (!CopyFileA(srcpath, dstpath, FALSE)) {

		create_path(dstpath);
		return CopyFileA(srcpath, dstpath, FALSE);
	}
	
	return TRUE;
}

b8 file_exists(const char* filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	
	DWORD att = GetFileAttributes(filepath);
	if(INVALID_FILE_ATTRIBUTES == att)
	{
		return FALSE;
	}
	return TRUE;
}

b8 folder_create(const char* filepath_, b8 recursive)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);

	if (recursive) create_path(filepath);
	return CreateDirectory(filepath, NULL);
}

b8 folder_remove(const char* filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);

	return RemoveDirectoryA(filepath);
}

inline FolderElement finddata_to_folderelement(WIN32_FIND_DATAA d)
{
	FolderElement e;
	e.is_file = !(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	e.create_date = filetime_to_date(d.ftCreationTime);
	e.last_write_date = filetime_to_date(d.ftLastWriteTime);
	e.last_access_date = filetime_to_date(d.ftLastAccessTime);
	sprintf(e.name, "%s", (const char*)d.cFileName);
	size_t size = strlen(e.name);
	if (size == 0u) e.extension = NULL;
	else {
		const char* begin = e.name;
		const char* it = begin + size;

		while (it != begin && *it != '.') {
			--it;
		}

		if (it == begin) {
			e.extension = NULL;
		}
		else {
			assert(*it == '.');
			e.extension = it + 1u;
		}
	}
	return e;
}

b8 folder_iterator_begin(const char* folderpath__, FolderIterator* iterator, FolderElement* element)
{
	WIN32_FIND_DATAA data;

	char folderpath_[MAX_PATH];
	filepath_resolve(folderpath_, folderpath__);

	// Clear path
	char folderpath[MAX_PATH];
	
	const char* it = folderpath_;
	char* it0 = folderpath;

	if (!path_is_absolute(folderpath_)) {
		*it0++ = '.';
		*it0++ = '\\';
	}
	
	while (*it != '\0') {
		if (*it == '/') *it0 = '\\';
		else *it0 = *it;
	    
		++it;
		++it0;
	}

	if (*(it0 - 1u) != '\\')
		*it0++ = '\\';
	*it0++ = '*';
	*it0++ = '\0';
	

	HANDLE find = FindFirstFileA(folderpath, &data);
	
	if (find == INVALID_HANDLE_VALUE) return FALSE;

	*element = finddata_to_folderelement(data);
	iterator->_handle = (u64)find;

	return TRUE;
}
    
b8 folder_iterator_next(FolderIterator* iterator, FolderElement* element)
{
	HANDLE find = (HANDLE)iterator->_handle;
	
	WIN32_FIND_DATAA data;
	if (FindNextFileA(find, &data)) {

		*element = finddata_to_folderelement(data);
		return TRUE;
	}

	return FALSE;
}

void folder_iterator_close(FolderIterator* iterator)
{
	HANDLE find = (HANDLE)iterator->_handle;
	FindClose(find);
}

//////////////////////////////// MULTITHREADING ////////////////////////////

Mutex mutex_create()
{
	Mutex mutex;
	mutex._handle = (u64)CreateMutexA(NULL, FALSE, NULL);
	return mutex;
}
    
void mutex_destroy(Mutex mutex)
{
	if (mutex._handle != NULL) {
		CloseHandle((HANDLE)mutex._handle);
	}
}

void mutex_lock(Mutex mutex)
{
	assert(mutex._handle != 0u);
	WaitForSingleObject((HANDLE)mutex._handle, INFINITE);
}
    
void mutex_unlock(Mutex mutex)
{
	assert(mutex._handle != 0u);
	ReleaseMutex((HANDLE)mutex._handle);
}

// DYNAMIC LIBRARIES

Library library_load(const char* filepath_)
{
	char filepath[MAX_PATH];
	filepath_resolve(filepath, filepath_);
	return (Library) LoadLibrary(filepath);
}
	
void library_free(Library library)
{
	FreeLibrary((HINSTANCE)library);
}

void* library_address(Library library, const char* name)
{
	return GetProcAddress((HINSTANCE)library, name);
}
