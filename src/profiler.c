#include "Hosebase/profiler.h"

#if SV_SLOW

typedef struct {

	ProfilerFunctionData* functions;
	u32 function_count;
	u32 function_capacity;

	Mutex mutex;

} ProfilerData;

static ProfilerData* profiler;

void _profiler_initialize()
{
	profiler = memory_allocate(sizeof(ProfilerData));
	profiler->mutex = mutex_create();
}

void _profiler_reset()
{
	foreach(i, profiler->function_count)
	{
		ProfilerFunctionData* fn = profiler->functions + i;

		fn->total_time = 0.0;
		fn->current_frame++;

		ProfilerFunctionFrame* frame = fn->frames + (fn->current_frame % PROFILER_FUNCTION_CACHE);
		frame->calls = 0;
		frame->total_time = 0.0;
	}
}

void _profiler_close()
{
	if (profiler) {

		mutex_destroy(profiler->mutex);

		memory_free(profiler);
	}
}

// Set the name parameter to create if not exists
inline ProfilerFunctionData* new_function(u64 hash, const char* name)
{
	array_prepare(&profiler->functions, &profiler->function_count,
		&profiler->function_capacity, profiler->function_capacity + 200, 1, sizeof(ProfilerFunctionData));

	ProfilerFunctionData* fn = profiler->functions + profiler->function_count++;

	foreach(i, PROFILER_FUNCTION_CACHE) {

		ProfilerFunctionFrame* frame = fn->frames + i;
		frame->calls = 0;
		frame->total_time = 0.0;
	}
	fn->current_frame = 0;
	fn->total_time = 0.0;

	string_copy(fn->name, name, PROFILER_FN_NAME_SIZE);
	fn->hash = hash;

	return fn;
}

inline ProfilerFunctionData* find_function(u64 hash)
{
	foreach(i, profiler->function_count) {

		ProfilerFunctionData* fn = profiler->functions + i;
		if (fn->hash == hash)
			return fn;
	}

	return NULL;
}

void _profiler_save(const char* name, struct _ProfilerChrono res, b8 is_function)
{
	u64 hash = hash_string(name);

	mutex_lock(profiler->mutex);

	ProfilerFunctionData* fn = find_function(hash);
	if (fn == NULL) fn = new_function(hash, name);

	fn->is_function = is_function;
	
	ProfilerFunctionFrame* frame = fn->frames + (fn->current_frame % PROFILER_FUNCTION_CACHE);

	f64 ellapsed = res.end - res.begin;

	frame->total_time += ellapsed;
	frame->calls++;

	mutex_unlock(profiler->mutex);
}

void profiler_lock()
{
	mutex_lock(profiler->mutex);
}

void profiler_unlock()
{
	mutex_unlock(profiler->mutex);
}

u32 profiler_function_count()
{
	return profiler->function_count;
}

ProfilerFunctionData* profiler_function_get(u32 index)
{
	return profiler->functions + index;
}

void profiler_sort(LessThanFn fn)
{
	array_sort(profiler->functions, profiler->function_count, sizeof(ProfilerFunctionData), fn);
}

#endif