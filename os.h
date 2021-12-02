#pragma once

#include "Hosebase/allocators.h"
#include "Hosebase/math.h"

SV_BEGIN_C_HEADER

void filepath_resolve(char* dst, const char* src);
void filepath_user(char* dst);
	
void print(const char* str, ...);

void show_message(const char* title, const char* content, b8 error);
b8   show_dialog_yesno(const char* title, const char* content); // Returns true if yes

b8 file_dialog_open(char* buff, u32 filterCount, const char** filters, const char* startPath);
b8 file_dialog_save(char* buff, u32 filterCount, const char** filters, const char* startPath);

// Timer

typedef struct {

	u32 year;
	u32 month;
	u32 day;
	u32 hour;
	u32 minute;
	u32 second;
	u32 millisecond;
		
} Date;

inline b8 date_equals(Date d0, Date d1)
{
	return d0.year == d1.year &&
		d0.month == d1.month &&
		d0.day == d1.day &&
		d0.hour == d1.hour &&
		d0.minute == d1.minute &&
		d0.second == d1.second &&
		d0.millisecond == d1.millisecond;
}

f64  timer_now();
Date timer_date();

// Window

typedef enum {
	WindowState_Windowed,
	WindowState_Maximized,
	WindowState_Minimized,
	WindowState_Fullscreen
} WindowState;

u64         window_handle();
v2_u32      window_size();
f32         window_aspect();
WindowState window_state();
void        set_window_fullscreen(b8 fullscreen);

v2_u32 desktop_size();

// Cursor

void cursor_hide();
void cursor_show();
    
// File Management

b8 path_is_absolute(const char* path);
void path_clear(char* path);


b8 file_read_binary(const char* filepath, u8** data, u32* size);
b8 file_read_text(const char* filepath, u8** data, u32* size);
b8 file_write_binary(const char* filepath, const u8* data, size_t size, b8 append, b8 recursive);
b8 file_write_text(const char* filepath, const char* str, size_t size, b8 append, b8 recursive);

b8 file_remove(const char* filepath);
b8 file_copy(const char* srcpath, const char* dstpath);
b8 file_exists(const char* filepath);
b8 folder_create(const char* filepath, b8 recursive);
b8 folder_remove(const char* filepath);

b8 file_date(const char* filepath, Date* create, Date* last_write, Date* last_access);

typedef struct {
	Date        create_date;
	Date        last_write_date;
	Date        last_access_date;
	b8          is_file;
	char        name[FILE_NAME_SIZE + 1u];
	const char* extension;
} FolderElement;

typedef struct {
	u64 _handle;
	b8 has_next;
	FolderElement element;
} FolderIterator;

FolderIterator folder_iterator_begin(const char* folderpath);
void folder_iterator_next(FolderIterator* iterator);
void folder_iterator_close(FolderIterator* iterator);

#define foreach_file(it, path) for (FolderIterator it = folder_iterator_begin(path); it.has_next; folder_iterator_next(&it))

// CLIPBOARD

b8          clipboard_write_ansi(const char* text);
const char* clipboard_read_ansi();

// MULTITHREADING STUFF

typedef u64 Mutex;
typedef u64 Thread;

typedef i32(*ThreadMainFn)(void*);

#define TASK_DATA_SIZE 128

typedef void(*TaskFn)(void* user_data);

typedef struct {
	volatile i32 completed;
	u32 dispatched;
} TaskContext;
    
Mutex mutex_create();
void  mutex_destroy(Mutex mutex);

void mutex_lock(Mutex mutex);
void mutex_unlock(Mutex mutex);

#ifdef __cplusplus

struct _LockGuard {
	Mutex* m;
    _LockGuard(Mutex* mutex) : m(mutex) { mutex_lock(*m); }
	~_LockGuard() { mutex_unlock(*m); }
};

#define SV_LOCK_GUARD(mutex, name) _LockGuard name(&mutex);

#endif

Thread thread_create(ThreadMainFn main, void* data);
void   thread_destroy(Thread thread);
void   thread_wait(Thread thread);
void   thread_sleep(u64 millis);

typedef struct {
	TaskFn fn;
	const void* data;
	u32 size;
} TaskDesc;

void task_dispatch(TaskDesc* tasks, u32 task_count, TaskContext* context);
void task_wait(TaskContext* context);
b8 task_running(TaskContext* context);

u32 interlock_increment_u32(volatile u32* n);
u32 interlock_decrement_u32(volatile u32* n);

// DYNAMIC LIBRARIES

typedef u64 Library;

Library library_load(const char* filepath);
void    library_free(Library library);
void*   library_address(Library library, const char* name);

// INTERNAL

typedef struct {

	struct {
		b8 open;
		v2_u32 pos;
		v2_u32 size;
		const char* title;
	} window;

} OSInitializeDesc;
	
b8   _os_initialize(const OSInitializeDesc* desc);
void _os_close();
b8 _os_recive_input();

SV_END_C_HEADER
