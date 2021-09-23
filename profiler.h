#pragma once

#include "Hosebase/hosebase.h"

#if SV_SLOW

struct _ProfilerChrono {
	f64 begin;
	f64 end;
};

#define profiler_chrono_begin(name) struct _ProfilerChrono name; name.begin = timer_now()
#define profiler_chrono_end(name) name.end = timer_now()
#define profiler_chrono_get(name) (name.end - name.begin)
#define profiler_chrono_log_millis(name) print("[PROFILER] %s: %ums\n", #name, (u32)(profiler_chrono_get(name) * 1000.0))
#define profiler_chrono_log_micros(name) print("[PROFILER] %s: %u micros\n", #name, (u32)(profiler_chrono_get(name) * 1000000.0))
#define profiler_chrono_log_nanos(name) print("[PROFILER] %s: %uns\n", #name, (u32)(profiler_chrono_get(name) * 1000000000.0))

#else

#define profiler_chrono_begin(name)
#define profiler_chrono_end(name)
#define profiler_chrono_get(name) 0.0
#define profiler_chrono_log_millis(name)
#define profiler_chrono_log_micros(name)
#define profiler_chrono_log_nanos(name)


#endif