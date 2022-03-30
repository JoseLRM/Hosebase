#pragma once

#include "Hosebase/networking.h"
#include "Hosebase/memory_manager.h"
#include "Hosebase/os.h"

#if SV_NETWORKING

#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define ASSERTION_MESSAGES_MAX 10000
#define CLIENT_ASSERTION_MESSAGE_RATE 1.0
#define SERVER_ASSERTION_MESSAGE_RATE 5.0
#define WAIT_COUNT 50

typedef struct NetHeader NetHeader;

#define HEADER_TYPE_CONNECT 0x00
#define HEADER_TYPE_ACCEPT 0x12
#define HEADER_TYPE_CUSTOM 0x32
#define HEADER_TYPE_ASSERTION 0x90
#define HEADER_TYPE_DISCONNECT 0xFF

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

typedef struct {
	WSADATA winsock_data;
} NetData;

typedef struct {
	WebMessage* data;
	u32 count;
	u32 capacity;
	u32 tail;
} WebMessageQueue;

typedef struct {

	struct {
		u32 tail;
		u32 head;
		NetHeader* messages[ASSERTION_MESSAGES_MAX];	
	} recive;

	struct {
		NetHeader* messages[ASSERTION_MESSAGES_MAX];
		u32 tail;
		u32 head;
	} send;

    b8 send_request;
} MessageAssertion;

void message_queue_push(WebMessageQueue* queue, WebMessage msg);
b8 message_queue_pop(WebMessageQueue* queue, WebMessage* _msg);

b8 winsock_send(const void* data, u32 size, SOCKET socket, struct sockaddr_in dst);
NetHeader* winsock_recive_message(u8 *buffer, u32 buffer_capacity, SOCKET socket, u32 timeout, struct sockaddr *hint, i32 *hint_size);

b8 message_assertion_validate_send(MessageAssertion *assertion, NetHeader *msg);
b8 message_assertion_validate_recive(MessageAssertion *assertion, NetHeader *msg);

void message_assertion_send_request(MessageAssertion *assertion, b8 (*sendRequest)(void*, u32), void* data, SOCKET socket, struct sockaddr_in hint);
void message_assertion_recive_request(MessageAssertion *assertion, u32 assertion_count, SOCKET socket, struct sockaddr_in hint);

NetHeader* message_assertion_get_next_recived(MessageAssertion *assertion);

#endif