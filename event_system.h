#pragma once

#include "Hosebase/platform.h"

SV_BEGIN_C_HEADER

typedef void(*EventFn)(void* data);

u64 event_compute_handle(const char* system_name, u64 handle);

b8 event_register(u64 handle, const char* name, EventFn fn);

void event_unregister_handle(u64 handle);

void event_dispatch(u64 handle, const char* name, void* data);

b8 _event_initialize();
void _event_close();

SV_END_C_HEADER