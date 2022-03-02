#pragma once

#include "Hosebase/defines.h"

SV_BEGIN_C_HEADER

#if SV_SLOW

struct _ProfilerChrono {
	f64 begin;
	f64 end;
};

void _profiler_initialize();
void _profiler_reset();
void _profiler_close();

void _profiler_save(const char* name, struct _ProfilerChrono res, b8 is_function);

#define PROFILER_FN_NAME_SIZE 100

#define PROFILER_FUNCTION_CACHE 200

typedef struct {
	u32 calls;
	f64 total_time;
} ProfilerFunctionFrame;

typedef struct {

	ProfilerFunctionFrame frames[PROFILER_FUNCTION_CACHE];
	u32 current_frame;
	b8 is_function;

	f64 total_time; // Accumulation of frames

	char name[PROFILER_FN_NAME_SIZE];
	u64 hash;

} ProfilerFunctionData;

#define profiler_begin(name) struct _ProfilerChrono name; name.begin = timer_now()
#define profiler_end(name) do { name.end = timer_now(); _profiler_save(#name, name, FALSE); } while (0)

#define profiler_function_begin() struct _ProfilerChrono __function_profiler__; __function_profiler__.begin = timer_now()
#define profiler_function_end() do { __function_profiler__.end = timer_now(); _profiler_save(__FUNCTION__, __function_profiler__, TRUE); } while (0)

void profiler_lock();
void profiler_unlock();

u32 profiler_function_count();
ProfilerFunctionData* profiler_function_get(u32 index);
void profiler_sort(LessThanFn fn);

#else

#define profiler_begin(name)
#define profiler_end(name)

#define profiler_function_begin()
#define profiler_function_end()

#endif

SV_END_C_HEADER