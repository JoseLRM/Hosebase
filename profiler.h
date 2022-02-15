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

void profiler_gui();

#define profiler_begin(name) struct _ProfilerChrono name; name.begin = timer_now()
#define profiler_end(name) do { name.end = timer_now(); _profiler_save(#name, name, FALSE); } while (0)

#define profiler_function_begin() struct _ProfilerChrono __function_profiler__; __function_profiler__.begin = timer_now()
#define profiler_function_end() do { __function_profiler__.end = timer_now(); _profiler_save(__FUNCTION__, __function_profiler__, TRUE); } while (0)

#else

#define profiler_begin(name)
#define profiler_end(name)

#define profiler_function_begin()
#define profiler_function_end()

#endif

SV_END_C_HEADER