#pragma once

SV_BEGIN_C_HEADER

typedef void(*EventFn)(void* data);

b8 event_register(u64 handle, const char* name, EventFn fn);
b8 event_unregister(u64 handle, const char* name, EventFn fn);
b8 event_unregister_all(u64 handle, const char* name);

void event_dispatch(u64 handle, const char* name, void* data);

b8 _event_initialize();
void _event_close();

SV_END_C_HEADER