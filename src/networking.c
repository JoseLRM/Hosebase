#include "networking.h"

#include "Hosebase/os.h"

#if SV_NETWORKING

#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

typedef struct {
	u32 id;
	struct sockaddr_in hint;
} ClientRegister;

typedef struct {
	
	SOCKET socket;
	struct sockaddr_in hint;
	Thread thread;
	b8 running;
	u8* buffer;
	u32 buffer_capacity;

	ClientRegister* clients;
	u32 client_capacity;
	u32 client_id_count;

	WebServerAcceptFn accept_fn;
	WebServerDisconnectFn disconnect_fn;
	WebServerMessageFn message_fn;
	
} ServerData;

typedef struct {
	
	SOCKET socket;
	struct sockaddr_in server_hint;
	Thread thread;
	b8 running;
	u8* buffer;
	u32 buffer_capacity;
	u32 id;

	WebClientMessageFn message_fn;
	WebClientDisconnectFn disconnect_fn;

} ClientData;

typedef struct {
	
	WSADATA winsock_data;

	ServerData* server;
	ClientData* client;
	
} NetData;

#define HEADER_TYPE_CONNECT 0x00
#define HEADER_TYPE_ACCEPT 0x12
#define HEADER_TYPE_CUSTOM_FROM_SERVER 0x32
#define HEADER_TYPE_CUSTOM_FROM_CLIENT 0x69
#define HEADER_TYPE_DISCONNECT_CLIENT 0xFE
#define HEADER_TYPE_DISCONNECT_SERVER 0xFF

#pragma pack(push)
#pragma pack(1)

// TODO: Some data compression
typedef struct {
	u32 size;
	u8 type;
} NetHeader;

typedef struct {

	NetHeader header;
	u32 client_id;

} NetMessageAccept;

typedef struct {

	NetHeader header;
	u32 client_id;

} NetMessageDisconnectClient;

typedef struct {

	NetHeader header;
	u32 client_id;

} NetMessageCustom;

#pragma pack(pop)

static NetData* net;

/////////////////////////////////////////////// SERVER /////////////////////////////////////////////////////////

inline b8 _server_send(const void* data, u32 size, struct sockaddr_in dst)
{
	int ok = sendto(net->server->socket, data, size, 0, (struct sockaddr*)&dst, sizeof(dst));

	if (ok == SOCKET_ERROR) {
		SV_LOG_ERROR("Can't send data to the client\n");
		return FALSE;
	}

	return TRUE;
}

inline b8 _server_send_all(const void* data, u32 size, u32* clients_to_ignore, u32 client_count)
{
	ServerData* s = net->server;
	b8 res = TRUE;
	
	foreach(i, s->client_capacity) {

		b8 ignore = FALSE;

		if (s->clients[i].id == 0)
			continue;

		foreach(j, client_count) {

			if (s->clients[i].id == clients_to_ignore[j]) {
				ignore = TRUE;
				break;
			}
		}
		
		if (ignore) continue;

		if (!_server_send(data, size, s->clients[i].hint)) {
			res = FALSE;
		}
	}

	return res;
}

inline void register_client_if_not_exists(struct sockaddr_in client)
{
	ServerData* s = net->server;

	b8 found = FALSE;

	foreach(i, s->client_capacity) {

		if (s->clients[i].hint.sin_addr.S_un.S_addr == client.sin_addr.S_un.S_addr) {
			found = TRUE;
			break;
		}
	}

	if (!found) {

		u32 index = u32_max;

		foreach(i, s->client_capacity) {

			if (s->clients[i].id == 0) {
				index = i;
				break;
			}
		}

		if (index != u32_max) {

			ClientRegister reg;
			reg.hint = client;
			reg.id = ++s->client_id_count;
			
			char client_ip[256];
			memory_zero(client_ip, 256);
			inet_ntop(AF_INET, &client.sin_addr, client_ip, 256);

			b8 accept;

			if (s->accept_fn) {
				accept = s->accept_fn(reg.id, client_ip);
			}
			else {
				SV_LOG_INFO("Client Connected: '%s'\n", client_ip);
				accept = TRUE;
			}

			if (accept) {
				
				// Send accept message
				if (accept) {
					NetMessageAccept msg;

					msg.header.type = HEADER_TYPE_ACCEPT;
					msg.header.size = sizeof(NetMessageAccept) - sizeof(NetHeader);
					msg.client_id = reg.id;

					_server_send(&msg, sizeof(msg), client);
					
					s->clients[index] = reg;
				}
			}
		}
		else {
			
			SV_LOG_ERROR("Can't connect more clients, the limit is %u\n", s->client_capacity);
		}
	}
}

inline void unregister_client(u32 id)
{
	ServerData* s = net->server;

	if (id == 0) {
		SV_LOG_ERROR("Can't disconnect client '%u', invalid id\n", id);
	}

	u32 index = u32_max;
	
	foreach(i, s->client_capacity) {

		if (s->clients[i].id == id) {
			index = i;
			break;
		}
	}

	if (index != u32_max) {

		if (s->disconnect_fn)
			s->disconnect_fn(id);

		memory_zero(s->clients + index, sizeof(ClientRegister));
	}
	else {

		SV_LOG_ERROR("Can't disconnect client '%u', doesn't exist\n");
	}
}

inline NetHeader* _recive_message(u8* buffer, u32 buffer_capacity, SOCKET socket, u32 timeout, struct sockaddr* hint, i32* hint_size)
{
	memory_zero(buffer, buffer_capacity);

	b8 recived = FALSE;

	// Wait for message
	{
		FD_SET set;
		FD_ZERO(&set);
		FD_SET(socket, &set);

		struct timeval time;
		time.tv_sec = 0;
		time.tv_usec = timeout * 1000;

		i32 res = select(0, &set, NULL, NULL, &time);

		if (res > 0)
			recived = TRUE;
	}

	if (!recived) return NULL;

	u32 bytes = 0;

	while (bytes < sizeof(NetHeader)) {
		
		i32 res = recvfrom(socket, buffer + bytes, buffer_capacity, 0, hint, hint_size);
		
		if (res == SOCKET_ERROR) {

			SV_LOG_ERROR("Error reciving data\n");
			return NULL;
		}
		else if (res <= 0) {

			SV_LOG_ERROR("Invalid message format\n");
			return NULL;
		}

		bytes += res;
	}
	
	NetHeader* header = (NetHeader*)buffer;

	while (bytes < sizeof(NetHeader) + header->size) {

		i32 res = recvfrom(socket, buffer + bytes, buffer_capacity, 0, hint, hint_size);
		
		if (res == SOCKET_ERROR) {

			SV_LOG_ERROR("Error reciving data\n");
			return NULL;
		}
		else if (res <= 0) {

			SV_LOG_ERROR("Invalid message format\n");
			return NULL;
		}

		bytes += res;
	}

	return header;
}

inline NetHeader* server_recive_message(struct sockaddr_in* client, u32 timeout)
{
	i32 client_size = sizeof(*client);

	ServerData* s = net->server;

	return _recive_message(s->buffer, s->buffer_capacity, s->socket, timeout, (struct sockaddr*)client, &client_size);
}

static u32 server_loop(void* arg)
{
	struct sockaddr_in client;
	i32 client_size = sizeof(client);
	memory_zero(&client, client_size);

	ServerData* s = net->server;

	while (s->running) {

		NetHeader* header = server_recive_message(&client, 500);

		if (header) {
			
			switch (header->type) {

			case HEADER_TYPE_CONNECT:
				register_client_if_not_exists(client);
				break;
				
			case HEADER_TYPE_DISCONNECT_CLIENT:
			{
				NetMessageDisconnectClient* msg = (NetMessageDisconnectClient*)header;
				
				u32 id = msg->client_id;
				
				unregister_client(id);
			}
			break;
			
			case HEADER_TYPE_CUSTOM_FROM_CLIENT:
			{
				NetMessageCustom* msg = (NetMessageCustom*)header;
				
				u32 id = msg->client_id;

				// TODO: Check if is valid
				if (FALSE) {
					SV_LOG_ERROR("Can't recive a client message, the ID '%u' is not registred\n", id);
				}
				else {
					
					// Callback
					if (s->message_fn) {
						s->message_fn(id, msg + 1, header->size);
					}
				}
		
				/*char client_ip[256];
				memory_zero(client_ip, 256);
				inet_ntop(AF_INET, &client.sin_addr, client_ip, 256);
			
				SV_LOG_INFO("'%s': %u bytes\n", client_ip, header->size);*/
			}
			break;
			
			}
		}
	}

	return 0;
}

b8 web_server_initialize(u32 port, u32 client_capacity, u32 buffer_capacity, WebServerAcceptFn accept_fn, WebServerDisconnectFn disconnect_fn, WebServerMessageFn message_fn)
{
	net->server = memory_allocate(sizeof(ServerData));
	ServerData* s = net->server;
	
	// Create socket
	{
		s->socket = socket(AF_INET, SOCK_DGRAM, 0);

		if (s->socket == INVALID_SOCKET) {

			SV_LOG_ERROR("Can't create the server socket\n");
			goto error;
		}
	}

	// Fill hint
	{
		SV_ZERO(s->hint);

		s->hint.sin_addr.S_un.S_addr = ADDR_ANY;
		s->hint.sin_family = AF_INET;
		s->hint.sin_port = htons(port); // From little to big endian
	}

	// Bind socket

	if (bind(s->socket, (struct sockaddr*)&s->hint, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {

		SV_LOG_ERROR("Can't connect server socket\n");
		goto error;
	}

	// Initialize some data
	{
		s->running = TRUE;
		
		s->buffer_capacity = SV_MAX(buffer_capacity, 1000);
		s->buffer = memory_allocate(s->buffer_capacity);

		s->client_capacity = SV_MAX(client_capacity, 1);
		s->clients = memory_allocate(sizeof(ClientRegister) * s->client_capacity);
		
		s->accept_fn = accept_fn;
		s->disconnect_fn = disconnect_fn;
		s->message_fn = message_fn;
	}

	// Start server thread
	s->thread = thread_create(server_loop, NULL);

	if (s->thread == NULL)
		goto error;

	goto success;
error:
	if (s) {

		if (s->socket != INVALID_SOCKET) {
			closesocket(s->socket);
		}

		s->running = FALSE;

		if (s->buffer) memory_free(s->buffer);
		if (s->clients) memory_free(s->clients);

		if (s->thread) thread_destroy(s->thread);

		memory_free(s);
		net->server = NULL;
	}
	return FALSE;
success:
	return TRUE;
}

void web_server_close()
{
	ServerData* s = net->server;

	if (s) {
		s->running = FALSE;

		NetHeader header;
		header.type = HEADER_TYPE_DISCONNECT_SERVER;
		header.size = 0;

		_server_send_all(&header, sizeof(NetHeader), NULL, 0);
	
		thread_wait(s->thread);
		closesocket(s->socket);

		memory_free(s->buffer);
		memory_free(s->clients);

		memory_free(s);
		net->server = NULL;
	}
}

b8 web_server_send(u32* clients, u32 client_count, b8 ignore, const void* data, u32 size)
{
	ServerData* s = net->server;

	// TODO: Check if do multiple sends is more performant
	u32 buffer_size = sizeof(NetHeader) + size;
	u8* buffer = memory_allocate(buffer_size);

	NetHeader header;
	header.type = HEADER_TYPE_CUSTOM_FROM_SERVER;
	header.size = size;
	memory_copy(buffer, &header, sizeof(header));
	memory_copy(buffer + sizeof(header), data, size);

	b8 res = TRUE;
	
	if (ignore || client_count == 0) {

		res = _server_send_all(buffer, buffer_size, clients, ignore ? client_count : 0);
	}
	else {

		foreach(i, client_count) {

			if (!_server_send(buffer, buffer_size, s->clients[i].hint)) {
				res = FALSE;
			}
		}
	}

	memory_free(buffer);

	return res;
}

/////////////////////////////////////////////// CLIENT /////////////////////////////////////////////////////////

inline NetHeader* client_recive_message(u32 timeout)
{
	ClientData* c = net->client;

	return _recive_message(c->buffer, c->buffer_capacity, c->socket, timeout, NULL, NULL);
}

static u32 client_loop(void* arg)
{
	ClientData* c = net->client;
	
	while (c->running) {

		NetHeader* header = client_recive_message(500);

		if (header) {
			
			switch (header->type) {

			case HEADER_TYPE_ACCEPT:
				SV_LOG_ERROR("The connection is already connected\n");
				break;

			case HEADER_TYPE_CUSTOM_FROM_SERVER:
				if (c->message_fn) {
					c->message_fn(header + 1, header->size);
				}
				break;

			case HEADER_TYPE_DISCONNECT_SERVER:
				if (c->disconnect_fn) {
					c->disconnect_fn(DisconnectReason_ServerDisconnected);
				}
				c->running = FALSE;
				break;
			
			}
		}
	}

	return 0;
}

b8 _client_send(const void* data, u32 size)
{
	int ok = sendto(net->client->socket, data, size, 0, (struct sockaddr*)&net->client->server_hint, sizeof(net->client->server_hint));

	if (ok == SOCKET_ERROR) {
		SV_LOG_ERROR("Can't send data to the server\n");
		return FALSE;
	}

	return TRUE;
}

b8 web_client_initialize(const char* ip, u32 port, u32 buffer_capacity, WebClientMessageFn message_fn, WebClientDisconnectFn disconnect_fn)
{
	net->client = memory_allocate(sizeof(ClientData));
	ClientData* c = net->client;

	// Fill server hint
	{
		struct sockaddr_in server;
		SV_ZERO(server);
	
		server.sin_port = htons(port); // From little to big endian
		server.sin_family = AF_INET;
		inet_pton(AF_INET, ip, &server.sin_addr);

		c->server_hint = server;
	}

	// Init some data
	{
		c->running = TRUE;
		
		c->buffer_capacity = SV_MAX(buffer_capacity, 1000);
		c->buffer = memory_allocate(c->buffer_capacity);
		
		c->socket = socket(AF_INET, SOCK_DGRAM, 0);
		
		c->message_fn = message_fn;
		c->disconnect_fn = disconnect_fn;

		if (c->socket == INVALID_SOCKET) {
			SV_LOG_ERROR("Can't create client socket\n");
			goto error;
		}
	}

	// Send connection message
	{
		NetHeader msg;
		msg.type = HEADER_TYPE_CONNECT;
		msg.size = 0;

		if (!_client_send(&msg, sizeof(msg))) {
			SV_LOG_ERROR("Can't communicate with server\n");
			goto error;
		}
	}

	SV_LOG_INFO("Trying to connect to server\n");

	NetHeader* header = client_recive_message(5000);
	if (header && header->type == HEADER_TYPE_ACCEPT) {

		NetMessageAccept* msg = (NetMessageAccept*)header;
		c->id = msg->client_id;

		SV_LOG_INFO("Connection accepted, client id = %u\n", c->id);
	}
	else {
		SV_LOG_ERROR("Accept message don't recived\n");
		goto error;
	}

	// Start thread
	c->thread = thread_create(client_loop, NULL);
	
	if (c->thread == NULL) goto error;

	goto success;
error:
	if (c) {

		c->running = FALSE;
		
		if (c->socket != INVALID_SOCKET) closesocket(c->socket);
		if (c->buffer) memory_free(c->buffer);
		if (c->thread) thread_destroy(c->thread);
		
		memory_free(c);
		net->client = NULL;
	}

	return FALSE;
	
success:
	return TRUE;
}

void web_client_close()
{
	ClientData* c = net->client;

	if (c) {

		c->running = FALSE;
		memory_free(c->buffer);

		NetMessageDisconnectClient msg;
		msg.header.type = HEADER_TYPE_DISCONNECT_CLIENT;
		msg.header.size = sizeof(NetMessageDisconnectClient) - sizeof(NetHeader);
		msg.client_id = c->id;

		_client_send(&msg, sizeof(NetMessageDisconnectClient));

		thread_wait(c->thread);

		closesocket(c->socket);

		memory_free(c);
		net->client = NULL;
	}		
}

b8 web_client_send(const void* data, u32 size)
{
	NetMessageCustom msg;
	msg.header.type = HEADER_TYPE_CUSTOM_FROM_CLIENT;
	msg.header.size = size + sizeof(u32);
	msg.client_id = net->client->id;

	// TODO: Check if is more performat doing a dynamic allocation
	b8 res;
	res = _client_send(&msg, sizeof(msg));
	if (res) res =_client_send(data, size);

	return res;
}

b8 _net_initialize()
{
	net = memory_allocate(sizeof(NetData));
	
	i32 res = WSAStartup(MAKEWORD(2, 2), &net->winsock_data);

	if (res != 0) {

		SV_LOG_ERROR("Can't start WinSock\n");
		return FALSE;
	}

	return TRUE;
}

void _net_close()
{
	if (net) {

		if (net->server) {
			SV_LOG_ERROR("Server not closed\n");
		}
		if (net->client) {
			SV_LOG_ERROR("Client not closed\n");
		}
		
		WSACleanup();

		memory_free(net);
	}
}

#endif
