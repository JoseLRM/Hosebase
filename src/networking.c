#include "networking.h"

#include "Hosebase/os.h"

#if SV_NETWORKING

#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_ASSERTION_MESSAGES_MAX 10000
#define CLIENT_ASSERTION_MESSAGES_MAX 50000
#define ASSERTION_MESSAGE_RATE 1.0
#define WAIT_COUNT 50

typedef struct NetHeader NetHeader;

typedef struct {
	u32 id;
	struct sockaddr_in hint;
	b8 disconnect_request; // TODO

	u32 recive_assertion_count;
	u32 send_assertion_count;
	NetHeader* assertion_messages[SERVER_ASSERTION_MESSAGES_MAX];
	u32 assertion_messages_tail;
	u32 assertion_messages_head;
} ClientRegister;

typedef struct {
	WebMessage* data;
	u32 count;
	u32 capacity;
	u32 tail;
} WebMessageQueue;

inline void message_queue_push(WebMessageQueue* queue, WebMessage msg)
{
	array_prepare(&queue->data, &queue->count, &queue->capacity, queue->capacity + 500, 1, sizeof(WebMessage));
	queue->data[queue->count++] = msg;
}

inline b8 message_queue_pop(WebMessageQueue* queue, WebMessage* _msg)
{
	if (queue->tail >= queue->count)
		return FALSE;

	*_msg = queue->data[queue->tail++];

	if (queue->tail >= 100)
	{
		assert(queue->tail <= queue->count);

		for (u32 i = queue->tail; i < queue->count; ++i)
		{
			queue->data[i - queue->tail] = queue->data[i];
		}

		queue->count -= queue->tail;
		queue->tail = 0;
	}

	return TRUE;
}

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

	WebMessageQueue message_queue;

	Mutex mutex_send;
	Mutex mutex_message;
	
} ServerData;

typedef struct {
	
	SOCKET socket;
	struct sockaddr_in server_hint;
	Thread thread;
	b8 running;
	u8* buffer;
	u32 buffer_capacity;
	u32 id;

	u32 recive_assertion_count;
	u32 send_assertion_count;
	b8 send_assertion_message_request;
	f64 last_assertion_message;

	NetHeader* assertion_messages[CLIENT_ASSERTION_MESSAGES_MAX];
	u32 assertion_messages_tail;
	u32 assertion_messages_head;

	// TODO
	b8 disconnect_request;

	WebClientDisconnectFn disconnect_fn;

	Mutex mutex_send;
	Mutex mutex_message;

	WebMessageQueue message_queue;

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
#define HEADER_TYPE_ASSERTION_FROM_SERVER 0x90
#define HEADER_TYPE_ASSERTION_FROM_CLIENT 0x80
#define HEADER_TYPE_DISCONNECT_CLIENT 0xFE
#define HEADER_TYPE_DISCONNECT_SERVER 0xFF

#pragma pack(push)
#pragma pack(1)

// TODO: Some data compression
struct NetHeader {
	u32 size;
	u32 assert_value;
	u8 type;
};

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

typedef struct {

	NetHeader header;
	u32 client_id;
	u32 assertion_count;

} NetMessageClientAssertion;

typedef struct {

	NetHeader header;
	u32 assertion_count;

} NetMessageServerAssertion;

#pragma pack(pop)

static NetData* net;

void web_message_free(WebMessage* message)
{
	memory_free(message->data);
}

b8 _send(const void* data, u32 size, SOCKET socket, struct sockaddr_in dst)
{
	int ok = sendto(socket, data, size, 0, (struct sockaddr*) & dst, sizeof(dst));

	if (ok == SOCKET_ERROR)
	{
		i32 error_code = WSAGetLastError();

		char ip[256];
		inet_ntop(AF_INET, &dst.sin_addr, ip, 256);

		if (error_code == WSAEMSGSIZE)
			SV_LOG_ERROR("Can't send %u bytes to '%s'. The message is too large\n", size, ip);
		else
			SV_LOG_ERROR("Can't send %u bytes to '%s'. Error Code: %u\n", size, ip, error_code);

		return FALSE;
	}

	return TRUE;
}

inline b8 push_assertion_message(NetHeader** stack, u32* tail_, u32* head_, NetHeader* msg, u32 max)
{
	u32 tail = *tail_;
	u32 head = *head_;

	if (tail != head && (tail % max) == (head % max)) {
		return FALSE;
	}

	u32 index = head++ % max;

	stack[index] = msg;

	*tail_ = tail;
	*head_ = head;
	return TRUE;
}

/////////////////////////////////////////////// SERVER /////////////////////////////////////////////////////////

inline b8 server_assert_message(void* data, ClientRegister* client, b8 assert)
{
	ServerData* s = net->server;
	NetHeader* msg = data;

	if (assert) {

		msg->assert_value = client->send_assertion_count++;

		if (!push_assertion_message(client->assertion_messages, &client->assertion_messages_tail, &client->assertion_messages_head, msg, SERVER_ASSERTION_MESSAGES_MAX)) {

			SV_LOG_ERROR("The client %u has more than %u assert messages waiting, disconnecting...\n", client->id, SERVER_ASSERTION_MESSAGES_MAX);
			client->disconnect_request = TRUE;
			return FALSE;
		}
	}
	else msg->assert_value = u32_max;

	return TRUE;
}

inline b8 _server_send(void* data, u32 size, ClientRegister* client, b8 assert)
{
	ServerData* s = net->server;

	mutex_lock(s->mutex_send);

	if (server_assert_message(data, client, assert))
		_send(data, size, s->socket, client->hint);

	mutex_unlock(s->mutex_send);

	return TRUE;
}

inline b8 _server_send_all(void* data, u32 size, u32* clients_to_ignore, u32 client_count, b8 assert)
{
	ServerData* s = net->server;
	b8 res = TRUE;

	mutex_lock(s->mutex_send);
	
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

		if (server_assert_message(data, s->clients + i, assert)) {

			if (!_send(data, size, s->socket, s->clients[i].hint)) {
				res = FALSE;
			}
		}
	}

	mutex_unlock(s->mutex_send);

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
					msg.header.assert_value = u32_max;
					msg.header.size = sizeof(NetMessageAccept) - sizeof(NetHeader);
					msg.client_id = reg.id;

					mutex_lock(s->mutex_send);
					_send(&msg, sizeof(msg), s->socket, client);
					mutex_unlock(s->mutex_send);
					
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

	i32 res = recvfrom(socket, buffer, buffer_capacity, 0, hint, hint_size);

	if (res == SOCKET_ERROR) {

		SV_LOG_ERROR("Error reciving data\n");
		return NULL;
	}
	else if (res <= 0) {

		SV_LOG_ERROR("Invalid message format\n");
		return NULL;
	}
	else {

		NetHeader* header = (NetHeader*)buffer;

		if (header->size + sizeof(NetHeader) != res) {
			SV_LOG_ERROR("Unexpected message\n");
			return NULL;
		}

		return header;
	}
}

inline NetHeader* server_recive_message(struct sockaddr_in* client, u32 timeout)
{
	i32 client_size = sizeof(*client);

	ServerData* s = net->server;

	return _recive_message(s->buffer, s->buffer_capacity, s->socket, timeout, (struct sockaddr*)client, &client_size);
}

inline void _server_message_save(NetMessageCustom* header)
{
	ServerData* s = net->server;

	u32 size = header->header.size + sizeof(NetHeader) - sizeof(NetMessageCustom);

	WebMessage reg;
	reg.data = memory_allocate(size);
	memory_copy(reg.data, header + 1, size);

	reg.size = size;
	reg.client_id = header->client_id;

	mutex_lock(s->mutex_message);
	message_queue_push(&s->message_queue, reg);
	mutex_unlock(s->mutex_message);
}


static u32 server_loop(void* arg)
{
	struct sockaddr_in client_hint;
	i32 client_size = sizeof(client_hint);
	memory_zero(&client_hint, client_size);

	ServerData* s = net->server;

	while (s->running) {

		NetHeader* header = server_recive_message(&client_hint, WAIT_COUNT);

		ClientRegister* client = NULL;
		u32 client_id = u32_max;

		b8 send_assertion_message_request = FALSE;

		if (header) {

			client = NULL;
			client_id = *(u32*)(header + 1);
			{
				foreach(i, s->client_capacity) {

					if (s->clients[i].id == client_id) {
						client = s->clients + i;
						break;
					}
				}
			}

			if (client == NULL) {
				SV_LOG_ERROR("Can't recive a client message, the ID '%u' is not registred\n", client_id);
				continue;
			}

			if (header->assert_value != u32_max) {

				if (header->assert_value > client->recive_assertion_count) {
					send_assertion_message_request = TRUE;
					header = NULL;
					// TODO: Probably I don't want to lose this message, should be stored for future use
				}
				else if (header->assert_value < client->recive_assertion_count) {
					header = NULL;
				}
				else {
					client->recive_assertion_count++;
				}
			}
		}
		
		if (header) {

			switch (header->type) {

			case HEADER_TYPE_CONNECT:
				register_client_if_not_exists(client_hint);
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
				_server_message_save(msg);
			}
			break;

			case HEADER_TYPE_ASSERTION_FROM_CLIENT:
			{
				NetMessageClientAssertion* msg = (NetMessageClientAssertion*)header;

				u32 assertion_count = msg->assertion_count;

				// TODO: This can be a bottleneck
				mutex_lock(s->mutex_send);

				// Remove asserted messages from stack
				while (client->assertion_messages_tail < client->assertion_messages_head)
				{
					u32 index = client->assertion_messages_tail % SERVER_ASSERTION_MESSAGES_MAX;

					NetHeader* msg = client->assertion_messages[index];
					assert(msg);

					if (msg && msg->assert_value < assertion_count) {

						memory_free(msg);
						client->assertion_messages[index] = NULL;

						client->assertion_messages_tail++;
					}
					else break;
				}

				if (client->assertion_messages_tail == client->assertion_messages_head) {
					client->assertion_messages_tail = 0;
					client->assertion_messages_head = 0;
				}

				// Send again messages
				for (u32 i = client->assertion_messages_tail; i < client->assertion_messages_head; ++i) {

					u32 index = i % SERVER_ASSERTION_MESSAGES_MAX;

					NetHeader* msg = client->assertion_messages[index];
					assert(msg);

					if (msg) {
						_send(msg, sizeof(NetHeader) + msg->size, s->socket, client->hint);
					}
				}

				// Send assertion
				{
					NetMessageServerAssertion msg;
					msg.header.type = HEADER_TYPE_ASSERTION_FROM_SERVER;
					msg.header.size = sizeof(NetMessageServerAssertion) - sizeof(NetHeader);
					msg.header.assert_value = u32_max;
					msg.assertion_count = client->recive_assertion_count;

					_send(&msg, sizeof(NetMessageServerAssertion), s->socket, client->hint);
				}

				mutex_unlock(s->mutex_send);
			}
			break;
			
			}
		}

		if (send_assertion_message_request) {
			NetMessageServerAssertion msg;
			msg.header.type = HEADER_TYPE_ASSERTION_FROM_SERVER;
			msg.header.size = sizeof(NetMessageServerAssertion) - sizeof(NetHeader);
			msg.assertion_count = client->recive_assertion_count;

			_server_send(&msg, sizeof(NetMessageServerAssertion), client, FALSE);
		}
	}

	return 0;
}

b8 web_server_initialize(u32 port, u32 client_capacity, u32 buffer_capacity, WebServerAcceptFn accept_fn, WebServerDisconnectFn disconnect_fn)
{
	net->server = memory_allocate(sizeof(ServerData));
	ServerData* s = net->server;

	// Initialize some data
	{
		s->running = TRUE;

		s->buffer_capacity = SV_MAX(buffer_capacity, 1000);
		s->buffer = memory_allocate(s->buffer_capacity);

		s->client_capacity = SV_MAX(client_capacity, 1);
		s->clients = memory_allocate(sizeof(ClientRegister) * s->client_capacity);

		s->mutex_send = mutex_create();
		s->mutex_message = mutex_create();

		s->accept_fn = accept_fn;
		s->disconnect_fn = disconnect_fn;
	}
	
	// Create socket
	{
		s->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (s->socket == INVALID_SOCKET) {

			SV_LOG_ERROR("Can't create the server socket\n");
			goto error;
		}
	}

	int socket_buffer_size = s->buffer_capacity * 100;

	if (setsockopt(s->socket, SOL_SOCKET, SO_RCVBUF, (const char*)&socket_buffer_size, sizeof(int)) != 0) {
		SV_LOG_ERROR("Can't change the recive buffer size\n");
	}
	if (setsockopt(s->socket, SOL_SOCKET, SO_SNDBUF, (const char*)&socket_buffer_size, sizeof(int)) != 0) {
		SV_LOG_ERROR("Can't change the send buffer size\n");
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

		_server_send_all(&header, sizeof(NetHeader), NULL, 0, FALSE);
	
		thread_wait(s->thread);

		if (s->message_queue.data != NULL)
			memory_free(s->message_queue.data);

		mutex_destroy(s->mutex_send);
		mutex_destroy(s->mutex_message);

		closesocket(s->socket);

		memory_free(s->buffer);
		memory_free(s->clients);

		memory_free(s);
		net->server = NULL;
	}
}

b8 web_server_send(u32* clients, u32 client_count, b8 ignore, const void* data, u32 size, b8 assert)
{
	ServerData* s = net->server;

	// TODO: Use thread stack
	u32 buffer_size = sizeof(NetHeader) + size;
	u8* buffer = memory_allocate(buffer_size);

	NetHeader header;
	header.type = HEADER_TYPE_CUSTOM_FROM_SERVER;
	header.size = size;
	memory_copy(buffer, &header, sizeof(header));
	memory_copy(buffer + sizeof(header), data, size);

	b8 res = TRUE;
	
	if (ignore || client_count == 0) {

		res = _server_send_all(buffer, buffer_size, clients, ignore ? client_count : 0, assert);
	}
	else {

		foreach(i, client_count) {

			if (!_server_send(buffer, buffer_size, s->clients + i, assert)) {
				res = FALSE;
			}
		}
	}

	if (!assert)
		memory_free(buffer);

	return res;
}

b8 web_server_message_get(WebMessage* message)
{
	ServerData* s = net->server;

	b8 res;

	mutex_lock(s->mutex_message);
	res = message_queue_pop(&s->message_queue, message);
	mutex_unlock(s->mutex_message);

	return res;
}

b8 web_server_exists()
{
	return net->server != NULL;
}

/////////////////////////////////////////////// CLIENT /////////////////////////////////////////////////////////

inline NetHeader* client_recive_message(u32 timeout)
{
	ClientData* c = net->client;

	return _recive_message(c->buffer, c->buffer_capacity, c->socket, timeout, NULL, NULL);
}

inline void _client_message_save(const void* data, u32 size, u32 client_id)
{
	ClientData* c = net->client;

	WebMessage reg;
	reg.data = memory_allocate(size);
	memory_copy(reg.data, data, size);

	reg.size = size;
	reg.client_id = client_id;

	mutex_lock(c->mutex_message);
	message_queue_push(&c->message_queue, reg);
	mutex_unlock(c->mutex_message);
}

inline b8 client_assert_message(void* data, b8 assert)
{
	ClientData* c = net->client;
	NetHeader* msg = data;

	if (assert) {

		msg->assert_value = c->send_assertion_count++;

		if (!push_assertion_message(c->assertion_messages, &c->assertion_messages_tail, &c->assertion_messages_head, msg, CLIENT_ASSERTION_MESSAGES_MAX)) {

			SV_LOG_ERROR("The server has more than %u assert messages waiting, disconnecting...\n", CLIENT_ASSERTION_MESSAGES_MAX);
			c->disconnect_request = TRUE;
			return FALSE;
		}
	}
	else msg->assert_value = u32_max;

	return TRUE;
}

b8 _client_send(void* data, u32 size, b8 assert)
{
	ClientData* c = net->client;

	mutex_lock(c->mutex_send);
	if (client_assert_message(data, assert))
		_send(data, size, c->socket, net->client->server_hint);
	mutex_unlock(c->mutex_send);

	return TRUE;
}

static u32 client_loop(void* arg)
{
	ClientData* c = net->client;
	
	while (c->running) {

		NetHeader* header = client_recive_message(WAIT_COUNT);

		if (header) {

			if (header->assert_value != u32_max) {

				if (header->assert_value > c->recive_assertion_count) {
					c->send_assertion_message_request = TRUE;
					header = NULL;
					// TODO: Probably I don't want to lose this message, should be stored for future use
				}
				else if (header->assert_value < c->recive_assertion_count) {
					header = NULL;
				}
				else {
					c->recive_assertion_count++;
				}
			}
		}
		
		if (header) {

			switch (header->type) {

			case HEADER_TYPE_ACCEPT:
				SV_LOG_ERROR("The connection is already accepted\n");
				break;

			case HEADER_TYPE_CUSTOM_FROM_SERVER:
				_client_message_save(header + 1, header->size, 0);
				break;

			case HEADER_TYPE_DISCONNECT_SERVER:
				if (c->disconnect_fn) {
					c->disconnect_fn(DisconnectReason_ServerDisconnected);
				}
				c->running = FALSE;
				break;

			case HEADER_TYPE_CUSTOM_FROM_CLIENT:
			{
				NetMessageCustom* msg = (NetMessageCustom*)header;

				u32 id = msg->client_id;

				u32 size = msg->header.size + sizeof(NetHeader) - sizeof(NetMessageCustom);

				_client_message_save(msg + 1, size, msg->client_id);
			}
			break;

			case HEADER_TYPE_ASSERTION_FROM_SERVER:
			{
				NetMessageServerAssertion* msg = (NetMessageServerAssertion*)header;

				u32 assertion_count = msg->assertion_count;

				// TODO: This can be a bottleneck
				mutex_lock(c->mutex_send);

				// Remove asserted messages from stack
				while (c->assertion_messages_tail < c->assertion_messages_head)
				{
					u32 index = c->assertion_messages_tail % CLIENT_ASSERTION_MESSAGES_MAX;

					NetHeader* msg = c->assertion_messages[index];
					assert(msg);

					if (msg && msg->assert_value < assertion_count) {

						memory_free(msg);
						c->assertion_messages[index] = NULL;

						c->assertion_messages_tail++;
					}
					else break;
				}

				if (c->assertion_messages_tail == c->assertion_messages_head) {
					c->assertion_messages_tail = 0;
					c->assertion_messages_head = 0;
				}

				// Send again messages
				for (u32 i = c->assertion_messages_tail; i < c->assertion_messages_head; ++i) {

					u32 index = i % CLIENT_ASSERTION_MESSAGES_MAX;

					NetHeader* msg = c->assertion_messages[index];
					assert(msg);

					if (msg) {
						_send(msg, sizeof(NetHeader) + msg->size, c->socket, c->server_hint);
					}
				}

				mutex_unlock(c->mutex_send);
			}
			break;
			
			}
		}

		if (!c->send_assertion_message_request) {

			if (timer_now() - c->last_assertion_message > ASSERTION_MESSAGE_RATE) {
				c->send_assertion_message_request = TRUE;
			}
		}

		if (c->send_assertion_message_request) {

			NetMessageClientAssertion msg;
			msg.header.type = HEADER_TYPE_ASSERTION_FROM_CLIENT;
			msg.header.size = sizeof(NetMessageClientAssertion) - sizeof(NetHeader);

			msg.client_id = c->id;
			msg.assertion_count = c->recive_assertion_count;

			if (_client_send(&msg, sizeof(NetMessageClientAssertion), FALSE)) {

				c->send_assertion_message_request = FALSE;
				c->last_assertion_message = timer_now();
			}
		}
	}

	return 0;
}

b8 web_client_initialize(const char* ip, u32 port, u32 buffer_capacity, WebClientDisconnectFn disconnect_fn)
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
		
		c->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		
		c->disconnect_fn = disconnect_fn;

		c->mutex_send = mutex_create();
		c->mutex_message = mutex_create();

		if (c->socket == INVALID_SOCKET) {
			SV_LOG_ERROR("Can't create client socket\n");
			goto error;
		}
	}

	int socket_buffer_size = c->buffer_capacity * 100;

	if (setsockopt(c->socket, SOL_SOCKET, SO_RCVBUF, (const char*)&socket_buffer_size, sizeof(int)) != 0) {
		SV_LOG_ERROR("Can't change the recive buffer size\n");
	}
	if (setsockopt(c->socket, SOL_SOCKET, SO_SNDBUF, (const char*)&socket_buffer_size, sizeof(int)) != 0) {
		SV_LOG_ERROR("Can't change the send buffer size\n");
	}

	// Send connection message
	{
		NetHeader msg;
		msg.type = HEADER_TYPE_CONNECT;
		msg.size = 0;

		if (!_client_send(&msg, sizeof(msg), FALSE)) {
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

		_client_send(&msg, sizeof(NetMessageDisconnectClient), FALSE);

		thread_wait(c->thread);

		if (c->message_queue.data != NULL)
			memory_free(c->message_queue.data);

		mutex_destroy(c->mutex_send);
		mutex_destroy(c->mutex_message);

		closesocket(c->socket);

		memory_free(c);
		net->client = NULL;
	}		
}

inline b8 _web_client_send(NetMessageCustom msg, const void* data, u32 size, b8 assert)
{
	u8* mem = memory_allocate(sizeof(NetMessageCustom) + size);

	memory_copy(mem, &msg, sizeof(NetMessageCustom));
	memory_copy(mem + sizeof(NetMessageCustom), data, size);

	b8 res = _client_send(mem, sizeof(NetMessageCustom) + size, assert);

	if (!assert)
		memory_free(mem);

	return res;
}

b8 web_client_send(const void* data, u32 size, b8 assert)
{
	NetMessageCustom msg;
	msg.header.type = HEADER_TYPE_CUSTOM_FROM_CLIENT;
	msg.header.size = size + sizeof(u32);
	msg.client_id = net->client->id;

	return _web_client_send(msg, data, size, assert);
}

b8 web_client_message_get(WebMessage* message)
{
	ClientData* c = net->client;

	b8 res;

	mutex_lock(c->mutex_message);
	res = message_queue_pop(&c->message_queue, message);
	mutex_unlock(c->mutex_message);

	return res;
}

u32 web_client_id()
{
	return net->client ? net->client->id : 0;
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
