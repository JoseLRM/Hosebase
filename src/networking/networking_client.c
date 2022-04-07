#include "networking_internal.h"

#if SV_NETWORKING

typedef struct
{
	SOCKET socket;
	struct sockaddr_in server_hint;
	b8 running;
	u8 *buffer;
	u32 buffer_capacity;
	u32 id;

	TaskContext task_context;

	MessageAssertion assertion;

	// TODO
	b8 disconnect_request;

	WebClientDisconnectFn disconnect_fn;

	Mutex mutex_send;
	Mutex mutex_message;

	WebMessageQueue message_queue;

} ClientData;

static ClientData *client;

inline NetHeader *client_recive_message(u32 timeout)
{
	return winsock_recive_message(client->buffer, client->buffer_capacity, client->socket, timeout, NULL, NULL);
}

inline void _client_message_save(const void *data, u32 size, u32 client_id)
{
	ClientData *c = client;

	WebMessage reg;
	reg.data = memory_allocate(size);
	memory_copy(reg.data, data, size);

	reg.size = size;
	reg.client_id = client_id;

	mutex_lock(c->mutex_message);
	message_queue_push(&c->message_queue, reg);
	mutex_unlock(c->mutex_message);
}

inline b8 client_assert_message(void *data, b8 assert)
{
	ClientData *c = client;
	NetHeader *msg = data;

	if (assert)
	{
		if (!message_assertion_validate_send(&c->assertion, msg))
		{
			SV_LOG_ERROR("The server has more than %u assert messages waiting, disconnecting...\n", ASSERTION_MESSAGES_MAX);
			c->disconnect_request = TRUE;
			return FALSE;
		}
	}
	else
		msg->assert_value = u32_max;

	return TRUE;
}

b8 _client_send(void *data, u32 size, b8 assert)
{
	ClientData *c = client;

	b8 res = TRUE;

	mutex_lock(c->mutex_send);
	
	if (client_assert_message(data, assert))
		res = winsock_send(data, size, c->socket, client->server_hint);
	else 
		res = FALSE;

	mutex_unlock(c->mutex_send);

	return res;
}

static void client_process_message(NetHeader *header)
{
	ClientData *c = client;

	switch (header->type)
	{

	case HEADER_TYPE_ACCEPT:
		SV_LOG_ERROR("The connection is already accepted\n");
		break;

	case HEADER_TYPE_CUSTOM:
		_client_message_save(header + 1, header->size, 0);
		break;

	case HEADER_TYPE_DISCONNECT:
		if (c->disconnect_fn)
		{
			c->disconnect_fn(DisconnectReason_ServerDisconnected);
		}
		c->running = FALSE;
		break;

	case HEADER_TYPE_ASSERTION:
	{
		NetMessageServerAssertion *msg = (NetMessageServerAssertion *)header;

		u32 assertion_count = msg->assertion_count;

		// TODO: This can be a bottleneck
		mutex_lock(c->mutex_send);
		message_assertion_recive_request(&c->assertion, assertion_count, c->socket, c->server_hint);
		mutex_unlock(c->mutex_send);
	}
	break;
	}
}

inline b8 client_send_assertion_message(void *_, u32 count)
{
	NetMessageClientAssertion msg;
	msg.header.type = HEADER_TYPE_ASSERTION;
	msg.header.size = sizeof(NetMessageClientAssertion) - sizeof(NetHeader);

	msg.client_id = client->id;
	msg.assertion_count = count;

	return _client_send(&msg, sizeof(NetMessageClientAssertion), FALSE);
}

static u32 client_loop(void *arg)
{
	ClientData *c = client;

	while (c->running || message_assertion_has(&c->assertion))
	{
		NetHeader *header = client_recive_message(WAIT_COUNT);

		if (header)
		{
			if (!message_assertion_validate_recive(&c->assertion, header))
			{
				header = NULL;
			}
		}

		// Process messages
		{
			if (header == NULL)
				header = message_assertion_get_next_recived(&c->assertion);

			while (header != NULL)
			{
				client_process_message(header);
				header = message_assertion_get_next_recived(&c->assertion);
			}
		}

		message_assertion_send_request(&c->assertion, client_send_assertion_message, NULL, c->socket, c->server_hint);
	}

	return 0;
}

b8 web_client_initialize(const char *ip, u32 port, u32 buffer_capacity, WebClientDisconnectFn disconnect_fn)
{
	client = memory_allocate(sizeof(ClientData));
	ClientData *c = client;

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

		if (c->socket == INVALID_SOCKET)
		{
			SV_LOG_ERROR("Can't create client socket\n");
			goto error;
		}
	}

	int socket_buffer_size = c->buffer_capacity * 100;

	if (setsockopt(c->socket, SOL_SOCKET, SO_RCVBUF, (const char *)&socket_buffer_size, sizeof(int)) != 0)
	{
		SV_LOG_ERROR("Can't change the recive buffer size\n");
	}
	if (setsockopt(c->socket, SOL_SOCKET, SO_SNDBUF, (const char *)&socket_buffer_size, sizeof(int)) != 0)
	{
		SV_LOG_ERROR("Can't change the send buffer size\n");
	}

	// Send connection message
	{
		NetHeader msg;
		msg.type = HEADER_TYPE_CONNECT;
		msg.size = 0;

		if (!_client_send(&msg, sizeof(msg), FALSE))
		{
			SV_LOG_ERROR("Can't communicate with server\n");
			goto error;
		}
	}

	SV_LOG_INFO("Trying to connect to server\n");

	NetHeader *header = client_recive_message(5000);
	if (header && header->type == HEADER_TYPE_ACCEPT)
	{

		NetMessageAccept *msg = (NetMessageAccept *)header;
		c->id = msg->client_id;

		SV_LOG_INFO("Connection accepted, client id = %u\n", c->id);
	}
	else
	{
		SV_LOG_ERROR("Accept message don't recived\n");
		goto error;
	}

	// Start thread
	task_reserve_thread(client_loop, NULL, &c->task_context);

	goto success;
error:
	if (c)
	{
		c->running = FALSE;

		if (c->socket != INVALID_SOCKET)
			closesocket(c->socket);
		if (c->buffer)
			memory_free(c->buffer);

		memory_free(c);
		client = NULL;
	}

	return FALSE;

success:
	return TRUE;
}

void web_client_close()
{
	ClientData *c = client;

	if (c)
	{
		c->running = FALSE;
		memory_free(c->buffer);

		NetMessageDisconnectClient msg;
		msg.header.type = HEADER_TYPE_DISCONNECT;
		msg.header.size = sizeof(NetMessageDisconnectClient) - sizeof(NetHeader);
		msg.client_id = c->id;

		_client_send(&msg, sizeof(NetMessageDisconnectClient), TRUE);

		task_wait(&c->task_context);

		if (c->message_queue.data != NULL)
			memory_free(c->message_queue.data);

		mutex_destroy(c->mutex_send);
		mutex_destroy(c->mutex_message);

		closesocket(c->socket);

		memory_free(c);
		client = NULL;
	}
}

inline b8 _web_client_send(NetMessageCustom msg, const void *data, u32 size, b8 assert)
{
	u8 *mem = memory_allocate(sizeof(NetMessageCustom) + size);

	memory_copy(mem, &msg, sizeof(NetMessageCustom));
	memory_copy(mem + sizeof(NetMessageCustom), data, size);

	b8 res = _client_send(mem, sizeof(NetMessageCustom) + size, assert);

	if (!assert)
		memory_free(mem);

	return res;
}

b8 web_client_send(const void *data, u32 size, b8 assert)
{
	NetMessageCustom msg;
	msg.header.type = HEADER_TYPE_CUSTOM;
	msg.header.size = size + sizeof(u32);
	msg.client_id = client->id;

	return _web_client_send(msg, data, size, assert);
}

b8 web_client_message_get(WebMessage *message)
{
	ClientData *c = client;

	b8 res;

	mutex_lock(c->mutex_message);
	res = message_queue_pop(&c->message_queue, message);
	mutex_unlock(c->mutex_message);

	return res;
}

u32 web_client_id()
{
	return client ? client->id : 0;
}

b8 web_client_exists()
{
	return client != NULL;
}

#endif