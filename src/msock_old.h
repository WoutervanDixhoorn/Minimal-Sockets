#ifndef MSOCK_H
#define MSOCK_H

#include <ws2tcpip.h>
#include <winsock.h>

#include <assert.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    MSOCK_TYPE_CLIENT,
    MSOCK_TYPE_SERVER
} msock_type;

typedef enum {
    MSOCK_PROTOCOL_TCP,
    MSOCK_PROTOCOL_UDP
} msock_protocol;

typedef enum {
    MSOCK_STATE_DISCONNECTED,
    MSOCK_STATE_CONNECTED,
    
    MSOCK_STATE_UNBOUND,
    MSOCK_STATE_BOUND,
    MSOCK_STATE_LISTENING,
    MSOCK_STATE_ACCEPTED
} msock_state;

typedef struct {
    char* ip_addr;
    char* port;

    msock_type sock_type;
    msock_protocol sock_protocol;
} msock_info;

typedef struct {
    struct addrinfo* native_info;
    SOCKET native_socket;

    msock_type sock_type;
    msock_state sock_state;
} msock;

typedef struct {
    char* buffer;
    size_t size;  
    size_t len;
} msock_message;

bool msock_init();
bool msock_deinit();

bool msock_create(msock *result_socket, msock_info *info);
bool msock_destroy(msock *socket);
bool msock_connect(msock *socket);

bool msock_bind(msock *socket);
bool msock_listen(msock *socket);
bool msock_accept(msock *socket, msock *result_socket);
bool msock_client_close(msock *server, msock *client);

bool msock_send(msock *socket, msock_message *msg);
bool msock_receive(msock *socket, msock_message *result_msg);

#ifdef MSOCK_IMPLEMENTATION

bool msock_init() {
    WSADATA wsa_data;

    int success = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if(success != 0) {
        printf("WSAStartup failed: %d\n", success);
        return false;
    }
    
    return true;
}

bool msock_deinit() {
    WSACleanup();

    return true;
}

bool msock_create(msock *result_socket, msock_info *info) {
    struct addrinfo *ninfo = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //IPV4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = info->sock_protocol == MSOCK_PROTOCOL_TCP ? IPPROTO_TCP 
                      ? info->sock_protocol == MSOCK_PROTOCOL_UDP : IPPROTO_UDP 
                      : IPPROTO_TCP;
    if(info->sock_type == MSOCK_TYPE_SERVER) {
        hints.ai_flags = AI_PASSIVE;
    }

    int success = getaddrinfo(info->ip_addr, info->port, &hints, &ninfo);
    if(success != 0) {
        printf("getaddrinfo() failed; %d\n", WSAGetLastError());
        return false;
    }

    SOCKET client_socket = INVALID_SOCKET;
    client_socket = socket(ninfo->ai_family, ninfo->ai_socktype, ninfo->ai_protocol);
    if(client_socket == INVALID_SOCKET) {
        printf("socket() failed: %d\n", WSAGetLastError());
        freeaddrinfo(ninfo);
        return false;
    }

    result_socket->native_info = ninfo;
    result_socket->native_socket = client_socket;
    result_socket->sock_type = info->sock_type;
    msock_state type_state = info->sock_type == MSOCK_TYPE_CLIENT ? MSOCK_STATE_DISCONNECTED : MSOCK_STATE_UNBOUND;
    result_socket->sock_state = type_state;

    return true;
}

bool msock_destroy(msock *socket) {
    bool success = true;

    if(shutdown(socket->native_socket, SD_SEND) == SOCKET_ERROR) {
        printf("shutdown() failed: %d\n", WSAGetLastError());
        success = false;
    }

    freeaddrinfo(socket->native_info);
    closesocket(socket->native_socket);

    return success;
}

bool msock_connect(msock *socket) {
    assert(socket->sock_type == MSOCK_TYPE_CLIENT && "Only a socket of type MSOCK_TYPE_CLIENT can connect");
    assert(socket->sock_state != MSOCK_STATE_CONNECTED && "Cannot connect a already connected socket");
    assert(socket != NULL);

    int success = connect(socket->native_socket, socket->native_info->ai_addr, socket->native_info->ai_addrlen);
    if(success == SOCKET_ERROR) {
        printf("connect() failed: %d\n", WSAGetLastError());
        return false;
    }

    socket->sock_state = MSOCK_STATE_CONNECTED;

    return true;
}

bool msock_send(msock *socket, msock_message *msg) {
    int success = send(socket->native_socket, msg->buffer, msg->len, 0);
    if(success == SOCKET_ERROR) {
        printf("send() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

bool msock_receive(msock *socket, msock_message *result_msg) {
    if(socket->sock_state == MSOCK_STATE_LISTENING || socket->sock_state == MSOCK_STATE_DISCONNECTED) return false;

    int bytes_received = recv(socket->native_socket, result_msg->buffer, result_msg->size - 1, 0); //NOTE: recv is a blocking function

    if(bytes_received == 0) {
        printf("Connection closed!\n");

        if(socket->sock_type == MSOCK_TYPE_CLIENT) socket->sock_state = MSOCK_STATE_DISCONNECTED;
        else socket->sock_state = MSOCK_STATE_LISTENING;

        return false;
    }

    if(bytes_received < 0) {
        printf("recv() error: %d\n", WSAGetLastError());
        
        if(socket->sock_type == MSOCK_TYPE_CLIENT) socket->sock_state = MSOCK_STATE_DISCONNECTED;
        else socket->sock_state = MSOCK_STATE_LISTENING;

        return false;
    }
    
    result_msg->buffer[bytes_received] = '\0';
    result_msg->len = bytes_received;
    return true;
}

bool msock_bind(msock *socket) {
    assert(socket->sock_type == MSOCK_TYPE_SERVER && "msock_bind can only be called with a socket of type server");
    if(socket->sock_state == MSOCK_STATE_BOUND || socket->sock_state == MSOCK_STATE_LISTENING) return true;

    int success = bind(socket->native_socket, socket->native_info->ai_addr, (int)socket->native_info->ai_addrlen);
    if(success == SOCKET_ERROR) {
        printf("bind() failed: %d\n", WSAGetLastError());
        return false;
    }

    socket->sock_state = MSOCK_STATE_BOUND;

    return true;
}

bool msock_listen(msock *socket) {
    assert(socket->sock_type == MSOCK_TYPE_SERVER && "msock_listen can only be called with a socket of type server");
    if(socket->sock_state == MSOCK_STATE_ACCEPTED) return true; //NOTE: When connection has already been accepted no need to listen!
    if(!msock_bind(socket)) {
        printf("msock_bind() inside msock_listen() failed!\n");
        return false;
    }

    int success = listen(socket->native_socket, SOMAXCONN);
    if(success == SOCKET_ERROR) {
        printf("listen() failed: %d\n", WSAGetLastError());
        return false;
    }
    socket->sock_state = MSOCK_STATE_BOUND;

    return true;
}

bool msock_accept(msock *socket, msock *result_socket) {
    assert(socket->sock_type == MSOCK_TYPE_SERVER && "msock_accept can only be called with a socket of type server");
    if(socket->sock_state == MSOCK_STATE_ACCEPTED) return true;

    SOCKET accepted_socket = accept(socket->native_socket, NULL, NULL);
    if(accepted_socket == INVALID_SOCKET) {
        printf("accept() failed: %d\n", WSAGetLastError());
        return false;
    }

    result_socket->native_socket = accepted_socket;
    result_socket->sock_type = MSOCK_TYPE_CLIENT;
    result_socket->sock_state = MSOCK_STATE_CONNECTED;

    socket->sock_state = MSOCK_STATE_ACCEPTED;

    return true;
}

bool msock_client_close(msock *server, msock *client) 
{
    closesocket(client->native_socket);
    client->sock_state = MSOCK_STATE_DISCONNECTED;

    server->sock_state = MSOCK_STATE_LISTENING;

    return true;
}

#endif //MSOCK_IMPLEMTATION
#endif //MSOCK_H