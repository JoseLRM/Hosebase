#include "networking_internal.h"

#if SV_NETWORKING

static NetData *net;

b8 _net_initialize()
{
	net = memory_allocate(sizeof(NetData));

	i32 res = WSAStartup(MAKEWORD(2, 2), &net->winsock_data);

	if (res != 0)
	{

		SV_LOG_ERROR("Can't start WinSock\n");
		return FALSE;
	}

	return TRUE;
}

void _net_close()
{
	if (net)
	{
		if (web_server_exists())
		{
			SV_LOG_ERROR("Server not closed\n");
		}
		if (web_client_exists())
		{
			SV_LOG_ERROR("Client not closed\n");
		}

		WSACleanup();

		memory_free(net);
	}
}

/////////////////////////////////// WINSOCK UTILS /////////////////////////////////

b8 winsock_send(const void *data, u32 size, SOCKET socket, struct sockaddr_in dst)
{
	int ok = sendto(socket, data, size, 0, (struct sockaddr *)&dst, sizeof(dst));

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

NetHeader *winsock_recive_message(u8 *buffer, u32 buffer_capacity, SOCKET socket, u32 timeout, struct sockaddr *hint, i32 *hint_size)
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

	if (!recived)
		return NULL;

	i32 res = recvfrom(socket, buffer, buffer_capacity, 0, hint, hint_size);

	if (res == SOCKET_ERROR)
	{

		SV_LOG_ERROR("Error reciving data\n");
		return NULL;
	}
	else if (res <= 0)
	{

		SV_LOG_ERROR("Invalid message format\n");
		return NULL;
	}
	else
	{

		NetHeader *header = (NetHeader *)buffer;

		if (header->size + sizeof(NetHeader) != res)
		{
			SV_LOG_ERROR("Unexpected message\n");
			return NULL;
		}

		return header;
	}
}

///////////////////////////////// MESSAGE QUEUE //////////////////////////////////

void web_message_free(WebMessage *message)
{
	memory_free(message->data);
}

void message_queue_push(WebMessageQueue *queue, WebMessage msg)
{
	array_prepare(&queue->data, &queue->count, &queue->capacity, queue->capacity + 500, 1, sizeof(WebMessage));
	queue->data[queue->count++] = msg;
}

b8 message_queue_pop(WebMessageQueue *queue, WebMessage *_msg)
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

/////////////////////////////////////// MESSAGE ASSERTION ////////////////////////////////////

inline b8 message_assertion_push_send(MessageAssertion *assertion, NetHeader *msg)
{
	NetHeader **messages = assertion->send.messages;
	u32 tail = assertion->send.tail;
	u32 head = assertion->send.head;

	if (tail > head || head - tail >= ASSERTION_MESSAGES_MAX)
	{
		return FALSE;
	}

	u32 index = head++ % ASSERTION_MESSAGES_MAX;

	messages[index] = msg;
	assertion->send.tail = tail;
	assertion->send.head = head;

	return TRUE;
}

inline b8 message_assertion_push_recive(MessageAssertion *assertion, NetHeader *msg)
{
	NetHeader **messages = assertion->recive.messages;
	u32 tail = assertion->recive.tail;
	u32 head = assertion->recive.head;

	u32 target = msg->assert_value;

	assert(target < tail);

	if ((i32)head - (i32)target >= ASSERTION_MESSAGES_MAX)
	{
		return FALSE;
	}

	head = SV_MAX(target, head);

	u32 index = target % ASSERTION_MESSAGES_MAX;

	if (messages[index] != NULL)
		memory_free(messages[index]);

	messages[index] = msg;
	assertion->recive.tail = tail;
	assertion->recive.head = head;

	return TRUE;
}

b8 message_assertion_validate_send(MessageAssertion *assertion, NetHeader *msg)
{
	msg->assert_value = assertion->send.tail;
	return message_assertion_push_send(assertion, msg);
}

b8 message_assertion_validate_recive(MessageAssertion *assertion, NetHeader *msg)
{
	if (msg->assert_value != u32_max)
	{
		if (msg->assert_value > assertion->recive.tail)
		{
			assertion->send_request = TRUE;
			message_assertion_push_recive(assertion, msg);
			return TRUE;
		}
		else if (msg->assert_value < assertion->recive.tail)
		{
			// Ignore message, it's duplicated
			return FALSE;
		}
		else
		{
			if (assertion->recive.tail < assertion->recive.head)
			{
				u32 index = assertion->recive.tail % ASSERTION_MESSAGES_MAX;

				msg = assertion->recive.messages[index];

				if (msg != NULL)
				{
					memory_free(msg);
					assertion->recive.messages[index] = NULL;
				}

				assertion->recive.tail++;
			}
		}
	}

	return TRUE;
}

void message_assertion_send_request(MessageAssertion *assertion, b8 (*sendRequest)(void*, u32), void* data, SOCKET socket, struct sockaddr_in hint)
{
	NetHeader **messages = assertion->recive.messages;
	u32 tail = assertion->recive.tail;
	u32 head = assertion->recive.head;

	if (assertion->send_request)
	{
		if (tail < head)
		{
			u32 index = tail % ASSERTION_MESSAGES_MAX;
			assert(messages[index] == NULL);

			if (sendRequest(data, tail))
			{
				assertion->send_request = FALSE;
				SV_LOG_INFO("Send Request %u\n", tail);
			}
		}
	}

	assertion->send.tail = tail;
	assertion->send.head = head;
}

void message_assertion_recive_request(MessageAssertion *assertion, u32 assertion_count, SOCKET socket, struct sockaddr_in hint)
{
	NetHeader **messages = assertion->send.messages;
	u32 tail = assertion->send.tail;
	u32 head = assertion->send.head;

	// Remove asserted messages from stack
	while (tail < assertion_count)
	{
		u32 index = tail % ASSERTION_MESSAGES_MAX;

		NetHeader *msg = messages[index];

		assert(msg != NULL);
		assert(msg->assert_value < assertion_count);

		memory_free(msg);
		messages[index] = NULL;

		tail++;
	}

	// Send again message
	{
		u32 index = assertion_count % ASSERTION_MESSAGES_MAX;

		NetHeader *msg = messages[index];

		if_assert(msg != NULL)
		{
			winsock_send(msg, sizeof(NetHeader) + msg->size, socket, hint);
		}
	}

	assertion->send.tail = tail;
	assertion->send.head = head;
}

NetHeader *message_assertion_get_next_recived(MessageAssertion *assertion)
{
	NetHeader *msg = NULL;

	if (assertion->recive.tail < assertion->recive.head)
	{
		u32 index = assertion->recive.tail % ASSERTION_MESSAGES_MAX;

		msg = assertion->recive.messages[index];

		if (msg != NULL)
		{
			assertion->recive.messages[index] = NULL;
			assertion->recive.tail++;
		}
		else
		{
			assertion->send_request = TRUE;
		}
	}
	return msg;
}

b8 message_assertion_has(MessageAssertion *assertion)
{
	return assertion->send.tail < assertion->send.head;
}

#endif
