#pragma once

#include "Hosebase/defines.h"

#if SV_NETWORKING

#ifdef __cplusplus
extern "C" {
#endif

// WEB

typedef b8(*WebServerAcceptFn)(u32 client_id, const char* ip);
typedef void(*WebServerDisconnectFn)(u32 client_id);
typedef void(*WebServerMessageFn)(u32 client_id, const void* data, u32 size);

b8   web_server_initialize(u32 port, u32 client_capacity, u32 buffer_capacity, WebServerAcceptFn accept_fn, WebServerDisconnectFn disconnect_fn, WebServerMessageFn message_fn);
void web_server_close();
b8   web_server_send(u32* clients, u32 client_count, b8 ignore, const void* data, u32 size);

b8 web_server_exists();

typedef enum {
	DisconnectReason_Unknown,
	DisconnectReason_ServerDisconnected,
	DisconnectReason_Timeout,
} DisconnectReason;

typedef void(*WebClientMessageFn)(u32 sender_id, const void* data, u32 size);
typedef void(*WebClientDisconnectFn)(DisconnectReason reason);

b8   web_client_initialize(const char* ip, u32 port, u32 buffer_capacity, WebClientMessageFn message_fn, WebClientDisconnectFn disconnect_fn);
void web_client_close();
b8   web_client_send(const void* data, u32 size);
b8   web_client_send_all(const void* data, u32 size);

u32 web_client_id();

b8 _net_initialize();
void _net_close();

#endif

#ifdef __cplusplus
}
#endif
