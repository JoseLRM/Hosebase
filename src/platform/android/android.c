#include "Hosebase/platform.h"

#if SV_PLATFORM_ANDROID

#include "Hosebase/hosebase.h"

#include <jni.h>
#include <android_native_app_glue.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include <dlfcn.h>

typedef struct
{
    struct android_app *app;
    b8 close_request;
    b8 initialized;
    b8 resize;

    struct timespec start_timer;

    u32 window_width;
    u32 window_height;
} AndroidData;

static AndroidData *android;

b8 os_initialize(const PlatformInitializeDesc *desc)
{
    /*AConfiguration *config = AConfiguration_new();
    AConfiguration_fromAssetManager(config, android->app->activity->assetManager);
    AConfiguration_setOrientation(config, ACONFIGURATION_ORIENTATION_PORT);
    AConfiguration_delete(config);*/

    clock_gettime(CLOCK_MONOTONIC, &android->start_timer);

    ANativeActivity_setWindowFlags((ANativeActivity *)android->app->activity, 0x00000400 | 0x00000080 | 0x00000200, 0);

    return TRUE;
}

void os_close()
{
}

static void receive_messages()
{
    int ident;
    int events;
    struct android_poll_source *source;

    while ((ident = ALooper_pollAll(0, NULL, &events, (void **)&source)) >= 0)
    {
        // Process this event.
        if (source != NULL)
        {
            source->process(android->app, source);
        }
    }
}

b8 platform_recive_input()
{
    receive_messages();

    return !android->close_request;
}

void show_message(const char *title, const char *content, b8 error)
{
    // TODO:
}

b8 show_dialog_yesno(const char *title, const char *content)
{
    // TODO:
    return FALSE;
}

b8 file_dialog_open(char *buff, u32 filterCount, const char **filters, const char *startPath)
{
    // TODO:
    return FALSE;
}

b8 file_dialog_save(char *buff, u32 filterCount, const char **filters, const char *startPath)
{
    // TODO:
    return FALSE;
}

////////////////////////////// WINDOW ////////////////////////////

v2_u32 window_size()
{
    struct ANativeWindow *window = android->app->window;
    if (window == NULL)
        return (v2_u32){0, 0};

    u32 width = ANativeWindow_getWidth(window);
    u32 height = ANativeWindow_getHeight(window);

    return (v2_u32){width, height};
}

u64 window_handle()
{
    return (u64)android->app->window;
}

v2_u32 desktop_size()
{
    struct ANativeWindow *window = android->app->window;
    if (window == NULL)
        return (v2_u32){0, 0};

    u32 width = ANativeWindow_getWidth(window);
    u32 height = ANativeWindow_getHeight(window);
    return (v2_u32){width, height};
}

//////////////////////////// TIMER /////////////////////////

f64 timer_now()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    time_t seconds = (now.tv_sec - android->start_timer.tv_sec);
    long nanos = (now.tv_nsec - android->start_timer.tv_nsec);

    f64 time = (f64)seconds + ((f64)nanos / 1000000000.0);

    return time;
}

u64 timer_seed()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    u64 seed = now.tv_nsec;
    seed = hash_combine(seed, 0x38F68DA62D73ULL);
    seed = hash_combine(seed, now.tv_sec);

    return seed;
}

Date timer_date()
{
    // TODO:
    Date d;
    return d;
}

// File Management

void filepath_resolve(char *dst, const char *src, FilepathType type)
{
    if (path_is_absolute(src))
        string_copy(dst, src, FILE_PATH_SIZE);
    else
    {
        if (type == FilepathType_File)
        {
            string_copy(dst, android->app->activity->internalDataPath, FILE_PATH_SIZE);
            string_append(dst, "/", FILE_PATH_SIZE);
            string_append(dst, src, FILE_PATH_SIZE);
        }
        else
        {
            string_copy(dst, src, FILE_PATH_SIZE);
        }
    }
}

b8 file_read_binary(FilepathType type, const char *filepath_, u8 **data, u32 *size)
{
    char filepath[FILE_PATH_SIZE];
    filepath_resolve(filepath, filepath_, type);

    if (type == FilepathType_File)
    {
        FILE *file = fopen(filepath, "rb");

        if (file == NULL)
        {
            SV_LOG_ERROR("File '%s' not found\n", filepath);
            return FALSE;
        }

        fseek(file, 0, SEEK_END);
        *size = ftell(file);
        fseek(file, 0, SEEK_SET);

        *data = memory_allocate(*size);
        fread(*data, *size, 1, file);
        fclose(file);
    }
    else if (type == FilepathType_Asset)
    {
        AAssetManager *assets = android->app->activity->assetManager;

        AAsset *file = AAssetManager_open(assets, filepath, AASSET_MODE_BUFFER);

        if (file == NULL)
        {
            SV_LOG_ERROR("Asset file '%s' not found\n", filepath);
            return FALSE;
        }

        *size = (u32)AAsset_getLength(file);
        *data = memory_allocate(*size);

        AAsset_read(file, *data, *size);
        AAsset_close(file);
    }
    else
        return FALSE;

    return TRUE;
}

b8 file_read_text(FilepathType type, const char *filepath_, u8 **data, u32 *size)
{
    char filepath[FILE_PATH_SIZE];
    filepath_resolve(filepath, filepath_, type);

    if (type == FilepathType_File)
    {
        FILE *file = fopen(filepath, "r");

        if (file == NULL)
        {
            SV_LOG_ERROR("File '%s' not found\n", filepath);
            return FALSE;
        }

        fseek(file, 0, SEEK_END);
        *size = ftell(file);
        fseek(file, 0, SEEK_SET);

        *data = memory_allocate(*size + 1);
        fread(*data, *size, 1, file);
        (*data)[*size] = '\0';
        fclose(file);
    }
    else if (type == FilepathType_Asset)
    {
        AAssetManager *assets = android->app->activity->assetManager;
        if (assets == NULL)
            SV_LOG_INFO("Is null...\n");

        AAsset *file = AAssetManager_open(assets, filepath, AASSET_MODE_BUFFER);

        if (file == NULL)
        {
            SV_LOG_ERROR("Asset file '%s' not found\n", filepath);
            return FALSE;
        }

        *size = (u32)AAsset_getLength(file);
        *data = memory_allocate(*size + 1);

        AAsset_read(file, *data, *size);
        AAsset_close(file);
    }
    else
        return FALSE;

    (*data)[*size] = '\0';

    return TRUE;
}

SV_INLINE b8 create_path(const char *filepath)
{
    char folder[FILE_PATH_SIZE] = "\0";

    while (TRUE)
    {
        u32 folder_size = string_size(folder);

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

        if (mkdir(folder, S_IRWXU) != 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

b8 file_write_binary(FilepathType type, const char *filepath_, const u8 *data, size_t size, b8 append, b8 recursive)
{
    if (type == FilepathType_Asset)
    {
        SV_LOG_ERROR("Can't write a asset file '%s'\n", filepath_);
        return FALSE;
    }

    char filepath[FILE_PATH_SIZE];
    filepath_resolve(filepath, filepath_, type);

    const char *options = append ? "ab" : "wb";

    FILE *file = fopen(filepath, options);
    if (file == NULL)
    {
        if (recursive)
        {
            if (!create_path(filepath))
                return FALSE;

            file = fopen(filepath, options);
            if (file == NULL)
                return FALSE;
        }
        else
            return FALSE;
    }

    fwrite(data, 1, size, file);

    fclose(file);
    return TRUE;
}

b8 file_write_text(FilepathType type, const char *filepath_, const char *str, size_t size, b8 append, b8 recursive)
{
    if (type == FilepathType_Asset)
    {
        SV_LOG_ERROR("Can't write a asset file '%s'\n", filepath_);
        return FALSE;
    }

    char filepath[FILE_PATH_SIZE];
    filepath_resolve(filepath, filepath_, type);

    const char *options = append ? "a" : "w";

    FILE *file = fopen(filepath, options);
    SV_LOG_INFO("%s\n", (file == NULL) ? "Null" : "yes");
    if (file == NULL)
    {
        if (recursive)
        {
            if (!create_path(filepath))
                return FALSE;

            file = fopen(filepath, options);
            if (file == NULL)
                return FALSE;
        }
        else
            return FALSE;
    }

    fwrite(str, 1, size, file);

    fclose(file);
    return TRUE;
}

b8 file_remove(FilepathType type, const char *filepath)
{
    // TODO:
    return FALSE;
}

b8 file_copy(FilepathType type, const char *srcpath, const char *dstpath)
{
    // TODO:
    return FALSE;
}

b8 file_exists(FilepathType type, const char *filepath)
{
    // TODO:
    return FALSE;
}

b8 folder_create(FilepathType type, const char *filepath, b8 recursive)
{
    // TODO:
    return FALSE;
}

b8 folder_remove(FilepathType type, const char *filepath)
{
    // TODO:
    return FALSE;
}

b8 file_date(FilepathType type, const char *filepath, Date *create, Date *last_write, Date *last_access)
{
    // TODO:
    return FALSE;
}

////////////////////////// LIBRARIES ////////////////

Library library_load(FilepathType type, const char *filepath)
{
    return 0;
}

void library_free(Library library)
{
}

void *library_address(Library library, const char *name)
{
    void *module = NULL;
    // TODO: Use "library"

    void *fn = dlsym(module, name);

    if (fn == NULL)
    {
        SV_LOG_ERROR("%s\n", dlerror());
    }

    return fn;
}

/////////////////////////////// MULTITHREADING ///////////////////////

u32 interlock_increment_u32(volatile u32 *n)
{
    return __sync_fetch_and_add(n, 1);
}

u32 interlock_decrement_u32(volatile u32 *n)
{
    return __sync_fetch_and_add(n, -1);
}

Mutex mutex_create()
{
    // TODO: Thats bad
    pthread_mutex_t *mutex = memory_allocate(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, NULL);

    assert(sizeof(mutex) <= sizeof(Mutex));
    return (Mutex)mutex;
}

void mutex_destroy(Mutex mutex_)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    pthread_mutex_destroy(mutex);
    memory_free(mutex);
}

void mutex_lock(Mutex mutex_)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    pthread_mutex_lock(mutex);
}

b8 mutex_try_lock(Mutex mutex_)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    return pthread_mutex_trylock(mutex) == 0;
}

void mutex_unlock(Mutex mutex_)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    pthread_mutex_unlock(mutex);
}

Thread thread_create(ThreadMainFn main, void *data)
{
    assert_static(sizeof(pthread_t) <= sizeof(Thread));

    pthread_t thread;
    void *ret;

    if (pthread_create(&thread, NULL, main, "") != 0)
    {
        SV_LOG_ERROR("Can't create a thread\n");
        return 0;
    }

    return (Thread)thread;
}

void thread_destroy(Thread thread_)
{
    if (thread_ == 0)
        return;

    pthread_t thread = (pthread_t)thread_;
    // TODO: pthread_cancel(thread);
}

void thread_wait(Thread thread_)
{
    if (thread_ == 0)
        return;

    pthread_t thread = (pthread_t)thread_;

    if (pthread_join(thread, NULL) != 0)
    {
        SV_LOG_ERROR("Can't wait a thread\n");
    }
}

void thread_sleep(u64 millis)
{
    struct timespec ts;

    if (millis == 0)
    {
        return;
    }

    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000;

    int res;
    do
    {
        res = nanosleep(&ts, &ts);
    } while (res);
}

void thread_yield()
{
    // TODO: pthread_yield();
}

u64 thread_id()
{
    // TODO:
    return 0;
}

void thread_configure(Thread thread, const char *name, u64 affinity_mask, ThreadPrority priority)
{
}

//////////////////////////////// CLIPBOARD ////////////////////////////

b8 clipboard_write_ansi(const char *text)
{
    return FALSE;
}

const char *clipboard_read_ansi()
{
    return "";
}

/////////////////////////// ENTRY POINT ////////////////////////////

SV_INLINE v2 compute_touch_position(AInputEvent *event, u64 index)
{
    struct ANativeWindow *window = android->app->window;
    u32 width = ANativeWindow_getWidth(window);
    u32 height = ANativeWindow_getHeight(window);

    f32 x = AMotionEvent_getX(event, index);
    f32 y = AMotionEvent_getY(event, index);

    x = (x / (f32)width) - 0.5f;
    y = (y / (f32)height) - 0.5f;
    y = -y;

    return v2_set(x, y);
}

static int32_t engine_handle_input(struct android_app *app, AInputEvent *event)
{
    if (!android->initialized)
        return 0;

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        i32 action = AMotionEvent_getAction(event);
        i32 mouseAction = action & AMOTION_EVENT_ACTION_MASK;

        u64 current_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        InputState state = InputState_None;

        switch (mouseAction)
        {
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        {
            state = InputState_Released;
        }
        break;

        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        {
            state = InputState_Pressed;
        }
        break;
        }

        foreach (index, AMotionEvent_getPointerCount(event))
        {
            u32 id = AMotionEvent_getPointerId(event, index);
            v2 pos = compute_touch_position(event, index);

            _input_touch_set((index == current_index) ? state : InputState_None, pos, id);
        }

        return 1;
    }

    return 0;
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app *app, int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_SAVE_STATE:
        // The system has asked us to save our current state.  Do so.
        /*
        engine->app->savedState = malloc(sizeof(struct saved_state));
        *((struct saved_state*)engine->app->savedState) = engine->state;
        engine->app->savedStateSize = sizeof(struct saved_state);*/
        break;

    case APP_CMD_INIT_WINDOW:
        // The window is being shown, get it ready.
        if (android->app->window != NULL)
        {
            if (initialize())
            {
                android->initialized = TRUE;
            }
            else
                android->close_request = TRUE;
        }
        else
            android->close_request = TRUE;

        break;

    case APP_CMD_TERM_WINDOW:
        // The window is being hidden or closed, clean it up.
        android->close_request = TRUE;
        break;

    case APP_CMD_GAINED_FOCUS:
        // When our app gains focus
        break;

    case APP_CMD_LOST_FOCUS:
        // When our app loses focus
        break;

    default:
        break;
    }
}

static void engine_handle_resize(ANativeActivity *activity, ANativeWindow *window)
{
    android->resize = TRUE;
}

void android_main(struct android_app *state)
{
    android = (AndroidData *)memory_allocate(sizeof(AndroidData));

    state->userData = android;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    android->app = state;

    state->activity->callbacks->onNativeWindowResized = engine_handle_resize;

    while (!android->close_request)
    {

        if (android->initialized)
        {
            if (!hosebase_frame_begin())
                android->close_request = TRUE;

            if (android->resize)
            {
                android->resize = FALSE;
                graphics_swapchain_resize();
            }

            update();

            hosebase_frame_end();
        }
        else
        {
            receive_messages();
        }
    }

    close();
}

#endif