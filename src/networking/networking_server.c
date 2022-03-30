#include "networking_internal.h"

#if SV_NETWORKING

typedef struct
{
	u32 id;
	struct sockaddr_in hint;
	b8 disconnect_request; // TODO

	MessageAssertion assertion;
} ClientRegister;

typedef struct
{
	SOCKET socket;
	struct sockaddr_in hint;
	Thread thread;
	b8 running;
	u8 *buffer;
	u32 buffer_capacity;

	ClientRegister *clients;
	u32 client_capacity;
	u32 client_id_count;

	WebServerAcceptFn accept_fn;
	WebServerDisconnectFn disconnect_fn;

	WebMessageQueue message_queue;

	Mutex mutex_send;
	Mutex mutex_message;

} ServerData;

static ServerData *server;

inline b8 server_assert_message(void *data, ClientRegister *client, b8 assert)
{
	ServerData *s = server;
	NetHeader *msg = data;

	if (assert)
	{
		if (!message_assertion_validate_send(&client->assertion, msg))
		{
			SV_LOG_ERROR("The client %u has more than %u assert messages waiting, disconnecting...\n", client->id, ASSERTION_MESSAGES_MAX);
			client->disconnect_request = TRUE;
			return FALSE;
		}
	}
	else
		msg->assert_value = u32_max;

	return TRUE;
}

inline b8 _server_send(void *data, u32 size, ClientRegister *client, b8 assert)
{
	ServerData *s = server;

	mutex_lock(s->mutex_send);

	if (server_assert_message(data, client, assert))
		winsock_send(data, size, s->socket, client->hint);

	mutex_unlock(s->mutex_send);

	return TRUE;
}

inline b8 _server_send_all(void *data, u32 size, u32 *clients_to_ignore, u32 client_count, b8 assert)
{
	ServerData *s = server;
	b8 res = TRUE;

	mutex_lock(s->mutex_send);

	foreach (i, s->client_capacity)
	{

		b8 ignore = FALSE;

		if (s->clients[i].id == 0)
			continue;

		foreach (j, client_count)
		{

			if (s->clients[i].id == clients_to_ignore[j])
			{
				ignore = TRUE;
				break;
			}
		}

		if (ignore)
			continue;

		if (server_assert_message(data, s->clients + i, assert))
		{

			if (!winsock_send(data, size, s->socket, s->clients[i].hint))
			{
				res = FALSE;
			}
		}
	}

	mutex_unlock(s->mutex_send);

	return res;
}

inline void server_client_register_if_not_exists(struct sockaddr_in client)
{
	ServerData *s = server;

	b8 found = FALSE;

	foreach (i, s->client_capacity)
	{
		if (s->clients[i].hint.sin_addr.S_un.S_addr == client.sin_addr.S_un.S_addr)
		{
			found = TRUE;
			break;
		}
	}

	if (!found)
	{

		u32 index = u32_max;

		foreach (i, s->client_capacity)
		{

			if (s->clients[i].id == 0)
			{
				index = i;
				break;
			}
		}

		if (index != u32_max)
		{

			ClientRegister reg;
			reg.hint = client;
			reg.id = ++s->client_id_count;

			char client_ip[256];
			memory_zero(client_ip, 256);
			inet_ntop(AF_INET, &client.sin_addr, client_ip, 256);

			b8 accept;

			if (s->accept_fn)
			{
				accept = s->accept_fn(reg.id, client_ip);
			}
			else
			{
				SV_LOG_INFO("Client Connected: '%s'\n", client_ip);
				accept = TRUE;
			}

			if (accept)
			{

				// Send accept message
				if (accept)
				{
					NetMessageAccept msg;

					msg.header.type = HEADER_TYPE_ACCEPT;
					msg.header.assert_value = u32_max;
					msg.header.size = sizeof(NetMessageAccept) - sizeof(NetHeader);
					msg.client_id = reg.id;

					mutex_lock(s->mutex_send);
					winsock_send(&msg, sizeof(msg), s->socket, client);
					mutex_unlock(s->mutex_send);

					s->clients[index] = reg;
				}
			}
		}
		else
		{

			SV_LOG_ERROR("Can't connect more clients, the limit is %u\n", s->client_capacity);
		}
	}
}

inline ClientRegister *server_client_get(u32 client_id)
{
	ServerData *s = server;

	foreach (i, s->client_capacity)
	{
		if (s->clients[i].id == client_id)
		{
			return s->clients + i;
		}
	}

	return NULL;
}

inline void server_client_unregister(u32 id)
{
	ServerData *s = server;

	if (id == 0)
	{
		SV_LOG_ERROR("Can't disconnect client '%u', invalid id\n", id);
	}

	u32 index = u32_max;

	foreach (i, s->client_capacity)
	{
		if (s->clients[i].id == id)
		{
			index = i;
			break;
		}
	}

	if (index != u32_max)
	{
		if (s->disconnect_fn)
			s->disconnect_fn(id);

		memory_zero(s->clients + index, sizeof(ClientRegister));
	}
	else
	{
		SV_LOG_ERROR("Can't disconnect client '%u', doesn't exist\n", id);
	}
}

inline b8 server_client_send_assertion_message(ClientRegister* client, u32 count)
{
	NetMessageServerAssertion msg;
	msg.header.type = HEADER_TYPE_ASSERTION;
	msg.header.size = sizeof(NetMessageServerAssertion) - sizeof(NetHeader);
	msg.assertion_count = count;

	return _server_send(&msg, sizeof(NetMessageServerAssertion), client, FALSE);
}

inline NetHeader *server_recive_message(struct sockaddr_in *client, u32 timeout)
{
	i32 client_size = sizeof(*client);
	ServerData *s = server;
	return winsock_recive_message(s->buffer, s->buffer_capacity, s->socket, timeout, (struct sockaddr *)client, &client_size);
}

inline void _server_message_save(NetMessageCustom *header)
{
	ServerData *s = server;

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

static void server_process_message(NetHeader *header, ClientRegister *client, struct sockaddr_in client_hint)
{
	ServerData *s = server;

	switch (header->type)
	{

	case HEADER_TYPE_CONNECT:
		server_client_register_if_not_exists(client_hint);
		break;

	case HEADER_TYPE_DISCONNECT:
	{
		NetMessageDisconnectClient *msg = (NetMessageDisconnectClient *)header;

		u32 id = msg->client_id;

		server_client_unregister(id);
	}
	break;

	case HEADER_TYPE_CUSTOM:
	{
		NetMessageCustom *msg = (NetMessageCustom *)header;
		_server_message_save(msg);
	}
	break;

	case HEADER_TYPE_ASSERTION:
	{
		NetMessageClientAssertion *msg = (NetMessageClientAssertion *)header;

		u32 assertion_count = msg->assertion_count;

		mutex_lock(s->mutex_send);
		message_assertion_recive_request(&client->assertion, assertion_count, s->socket, client->hint);
		mutex_unlock(s->mutex_send);
	}
	break;
	}
}

static u32 server_loop(void *arg)
{
	struct sockaddr_in client_hint;
	i32 client_size = sizeof(client_hint);
	memory_zero(&client_hint, client_size);

	ServerData *s = server;

	while (s->running)
	{
		NetHeader *header = server_recive_message(&client_hint, WAIT_COUNT);

		ClientRegister *client = NULL;
		u32 client_id = u32_max;

		if (header != NULL)
		{
			client_id = *(u32 *)(header + 1);
			client = server_client_get(client_id);

			if (client == NULL)
			{
				SV_LOG_ERROR("Can't recive a client message, the ID '%u' is not registred\n", client_id);
				continue;
			}

			if (!message_assertion_validate_recive(&client->assertion, header))
			{
				header = NULL;
			}
		}

		// Process messages
		if (client != NULL)
		{
			if (header == NULL)
				header = message_assertion_get_next_recived(&client->assertion);
			
			while (header != NULL)
			{
				server_process_message(header, client, client_hint);
				header = message_assertion_get_next_recived(&client->assertion);
			}
		}

		if (client != NULL)
			message_assertion_send_request(&client->assertion, server_client_send_assertion_message, client, s->socket, client->hint);
	}

	return 0;
}

b8 web_server_initialize(u32 port, u32 client_capacity, u32 buffer_capacity, WebServerAcceptFn accept_fn, WebServerDisconnectFn disconnect_fn)
{
	server = memory_allocate(sizeof(ServerData));
	ServerData *s = server;

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

		if (s->socket == INVALID_SOCKET)
		{

			SV_LOG_ERROR("Can't create the server socket\n");
			goto error;
		}
	}

	int socket_buffer_size = s->buffer_capacity * 100;

	if (setsockopt(s->socket, SOL_SOCKET, SO_RCVBUF, (const char *)&socket_buffer_size, sizeof(int)) != 0)
	{
		SV_LOG_ERROR("Can't change the recive buffer size\n");
	}
	if (setsockopt(s->socket, SOL_SOCKET, SO_SNDBUF, (const char *)&socket_buffer_size, sizeof(int)) != 0)
	{
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

	if (bind(s->socket, (struct sockaddr *)&s->hint, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{

		SV_LOG_ERROR("Can't connect server socket\n");
		goto error;
	}

	// Start server thread
	s->thread = thread_create(server_loop, NULL);

	if (s->thread == NULL)
		goto error;

	goto success;
error:
	if (s)
	{

		if (s->socket != INVALID_SOCKET)
		{
			closesocket(s->socket);
		}

		s->running = FALSE;

		if (s->buffer)
			memory_free(s->buffer);
		if (s->clients)
			memory_free(s->clients);

		if (s->thread)
			thread_destroy(s->thread);

		memory_free(s);
		server = NULL;
	}
	return FALSE;
success:
	return TRUE;
}

void web_server_close()
{
	ServerData *s = server;

	if (s)
	{
		s->running = FALSE;

		NetHeader header;
		header.type = HEADER_TYPE_DISCONNECT;
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
		server = NULL;
	}
}

b8 web_server_send(u32 *clients, u32 client_count, b8 ignore, const void *data, u32 size, b8 assert)
{
	ServerData *s = server;

	// TODO: Use thread stack
	u32 buffer_size = sizeof(NetHeader) + size;
	u8 *buffer = memory_allocate(buffer_size);

	NetHeader header;
	header.type = HEADER_TYPE_CUSTOM;
	header.size = size;
	memory_copy(buffer, &header, sizeof(header));
	memory_copy(buffer + sizeof(header), data, size);

	b8 res = TRUE;

	if (ignore || client_count == 0)
	{

		res = _server_send_all(buffer, buffer_size, clients, ignore ? client_count : 0, assert);
	}
	else
	{

		foreach (i, client_count)
		{

			if (!_server_send(buffer, buffer_size, s->clients + i, assert))
			{
				res = FALSE;
			}
		}
	}

	if (!assert)
		memory_free(buffer);

	return res;
}

b8 web_server_message_get(WebMessage *message)
{
	ServerData *s = server;

	b8 res;

	mutex_lock(s->mutex_message);
	res = message_queue_pop(&s->message_queue, message);
	mutex_unlock(s->mutex_message);

	return res;
}

b8 web_server_exists()
{
	return server != NULL;
}

#endif