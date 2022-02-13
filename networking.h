#pragma once

#include "Hosebase/defines.h"

#if SV_NETWORKING

SV_BEGIN_C_HEADER

// WEB

typedef struct {
	u32 client_id;
	void* data;
	u32 size;
} WebMessage;

void web_message_free(WebMessage* message);

typedef b8(*WebServerAcceptFn)(u32 client_id, const char* ip);
typedef void(*WebServerDisconnectFn)(u32 client_id);

b8   web_server_initialize(u32 port, u32 client_capacity, u32 buffer_capacity, WebServerAcceptFn accept_fn, WebServerDisconnectFn disconnect_fn);
void web_server_close();
b8   web_server_send(u32* clients, u32 client_count, b8 ignore, const void* data, u32 size, b8 assert);

b8 web_server_message_get(WebMessage* message);

b8 web_server_exists();

typedef enum {
	DisconnectReason_Unknown,
	DisconnectReason_ServerDisconnected,
	DisconnectReason_Timeout,
} DisconnectReason;

typedef void(*WebClientDisconnectFn)(DisconnectReason reason);

b8   web_client_initialize(const char* ip, u32 port, u32 buffer_capacity, WebClientDisconnectFn disconnect_fn);
void web_client_close();
b8   web_client_send(const void* data, u32 size, b8 assert);

b8 web_client_message_get(WebMessage* message);

u32 web_client_id();

b8 _net_initialize();
void _net_close();

SV_END_C_HEADER

#endif