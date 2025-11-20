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
    MSOCK_TCP,
    MSOCK_UDP
} msock_protocol;

typedef enum {
    MSOCK_STATE_DISCONNECTED,
    MSOCK_STATE_CONNECTED,
    
    MSOCK_STATE_UNBOUND,
    MSOCK_STATE_BOUND,
    MSOCK_STATE_LISTENING,
    MSOCK_STATE_ACCEPTED
} msock_state;

typedef enum {
    MSOCK_SUCCESS,
    MSOCK_ERROR,

    MSOCK_NO_WORK
} msock_status;

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
    SOCKET native_socket;

    msock_protocol socket_protocol;
    msock_state socket_state;
} msock_client;

typedef struct {
    SOCKET native_socket;

    msock_protocol socket_protocol;
    msock_state socket_state;

    SOCKET native_client_socket;
} msock_server;

typedef struct {
    char* buffer;
    size_t size;  
    size_t len;
} msock_message;

bool msock_init();
bool msock_deinit();

bool msock_client_create(msock_client *client_result);
bool msock_client_connect(msock_client *client_socket, const char* ip, const char* port);
bool msock_client_is_connected(msock_client *client_socket);
bool msock_client_close(msock_client *client_socket);

bool msock_client_send(msock_client *client_socket, msock_message *msg);
bool msock_client_receive(msock_client *client_socket, msock_message *result_msg);

bool msock_server_create(msock_server *server_result);
bool msock_server_listen(msock_server *server_socket, const char* ip, const char* port);
bool msock_server_is_listening(msock_server *server_socket);
msock_status msock_server_accept(msock_server *server_socket);
bool msock_server_close(msock_server *server_socket);
bool msock_server_close_client(msock_server *server_socket);

#ifdef MSOCK_IMPLEMENTATION

//MSOCK_CLIENT Implementations

bool msock_client_create(msock_client *client_result) {

    SOCKET sock = INVALID_SOCKET;
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock == INVALID_SOCKET) {
        printf("socket() failed: %d\n", WSAGetLastError());
        return false;
    }

    client_result->native_socket = sock;
    client_result->socket_state = MSOCK_STATE_DISCONNECTED;

    return true;
}

bool msock_client_connect(msock_client *client_socket, const char* ip, const char* port) {
    msock_init();

    msock_client_create(client_socket);

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* info;
    getaddrinfo(ip, port, &hints, &info);

    bool success = connect(client_socket->native_socket, info->ai_addr, (int)info->ai_addrlen) == 0;
    freeaddrinfo(info);

    if(success) {
        client_socket->socket_state = MSOCK_STATE_CONNECTED;
    }

    return success;
}

bool msock_client_is_connected(msock_client *client_socket) {
    return client_socket->socket_state == MSOCK_STATE_CONNECTED;
}

bool msock_client_close(msock_client *client_socket) {
    bool success = true;
    if(shutdown(client_socket->native_socket, SD_SEND) == SOCKET_ERROR) {
        printf("shutdown() failed: %d\n", WSAGetLastError());
        success = false;
    }

    closesocket(client_socket->native_socket);

    msock_deinit();

    return success;
}

bool msock_client_send(msock_client *client_socket, msock_message *msg) {
    int success = send(client_socket->native_socket, msg->buffer, msg->len, 0);
    if(success == SOCKET_ERROR) {
        printf("send() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

bool msock_client_receive(msock_client *client_socket, msock_message *result_msg) {
    if(client_socket->socket_state == MSOCK_STATE_DISCONNECTED) return false;

    int bytes_received = recv(client_socket->native_socket, result_msg->buffer, result_msg->size - 1, 0); //NOTE: recv is a blocking function

    if(bytes_received == 0) {
        printf("Connection closed!\n");
        client_socket->socket_state = MSOCK_STATE_DISCONNECTED;
        return false;
    }

    if(bytes_received < 0) {
        printf("Connection lost: %d\n", WSAGetLastError());
        client_socket->socket_state = MSOCK_STATE_DISCONNECTED;
        return false;
    }
    
    result_msg->buffer[bytes_received] = '\0';
    result_msg->len = bytes_received;
    return true;
}

//MSOCK_SERVER Implementations

bool msock_server_create(msock_server *server_result) {
    SOCKET sock = INVALID_SOCKET;
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock == INVALID_SOCKET) {
        printf("socket() failed: %d\n", WSAGetLastError());
        return false;
    }

    server_result->native_socket = sock;
    server_result->socket_state = MSOCK_STATE_UNBOUND;

    return true;
}

bool msock_server_listen(msock_server *server_socket, const char* ip, const char* port) {
    msock_init();

    msock_server_create(server_socket);

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* info;
    getaddrinfo(ip, port, &hints, &info);

    int success = bind(server_socket->native_socket, info->ai_addr, (int)info->ai_addrlen);
    if(success == SOCKET_ERROR) {
        printf("bind() failed: %d\n", WSAGetLastError());
        return false;
    }
    freeaddrinfo(info);

    success = listen(server_socket->native_socket, SOMAXCONN);
    if(success == SOCKET_ERROR) {
        printf("listen() failed: %d\n", WSAGetLastError());
        return false;
    }

    server_socket->socket_state = MSOCK_STATE_LISTENING;

    return true;
}

bool msock_server_is_listening(msock_server *server_socket) {
    return server_socket->socket_state == MSOCK_STATE_LISTENING;
}

msock_status msock_server_accept(msock_server *server_socket) {
    server_socket->native_client_socket = accept(server_socket->native_socket, NULL, NULL);
    if(server_socket->native_client_socket == INVALID_SOCKET) {
        printf("accept() failed: %d\n", WSAGetLastError());
        return MSOCK_ERROR;
    }

    return MSOCK_SUCCESS;
}

bool msock_server_close(msock_server *server_socket) {
    bool success = true;
    if(shutdown(server_socket->native_client_socket, SD_SEND) == SOCKET_ERROR) {
        printf("shutdown() failed: %d\n", WSAGetLastError());
        success = false;
    }

    closesocket(server_socket->native_client_socket);
    closesocket(server_socket->native_socket);

    msock_deinit();

    return success;
}

bool msock_server_close_client(msock_server *server_socket) {
    SOCKET client_sock = server_socket->native_client_socket;

    if (client_sock == INVALID_SOCKET) return false;

    if (shutdown(client_sock, SD_BOTH) == SOCKET_ERROR) {
        printf("shutdown() failed: %d\n", WSAGetLastError()); 
        return false;
    }

    closesocket(client_sock); 
    server_socket->native_client_socket = INVALID_SOCKET;

    return true;
}

bool msock_server_send(msock_server *server_socket, msock_message *msg) {
    if(server_socket->native_client_socket == INVALID_SOCKET) return false;

    int success = send(server_socket->native_client_socket, msg->buffer, msg->len, 0);
    if(success == SOCKET_ERROR) {
        printf("send() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;    
}

bool msock_server_receive(msock_server *server_socket, msock_message *result_msg) {
    if(server_socket->socket_state != MSOCK_STATE_LISTENING) return false;

    int bytes_received = recv(server_socket->native_client_socket, result_msg->buffer, result_msg->size - 1, 0); //NOTE: recv is a blocking function

    if(bytes_received == 0) {
        printf("Connection closed!\n");
        return false;
    }

    if(bytes_received < 0) {
        printf("Connection lost: %d\n", WSAGetLastError());
        return false;
    }
    
    result_msg->buffer[bytes_received] = '\0';
    result_msg->len = bytes_received;
    return true;
}

//BASE UTIL

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

#endif //MSOCK_IMPLEMTATION
#endif //MSOCK_H