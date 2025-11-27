#ifndef MSOCK_H
#define MSOCK_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <ws2tcpip.h>
    #include <winsock.h>

    typedef int socklen_t;
#else
#include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>

    //Map Types
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1

    //Map Functions
    #define closesocket close
    #define WSAGetLastError() (errno)
    
    //Map Constants
    #define WSAEWOULDBLOCK EWOULDBLOCK
    #define SD_SEND SHUT_WR 
    #define SD_BOTH SHUT_RDWR
#endif

#define MSOCK_MAX_CLIENTS 64

typedef struct msock_client msock_client;
typedef struct msock_server msock_server;

typedef bool (*msock_on_connect_cb)(msock_client *client);
typedef bool (*msock_on_disconnect_cb)(msock_client *client);
typedef bool (*msock_on_client_cb)(msock_server *server, msock_client *client);

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

struct msock_client {
    SOCKET native_socket;
    msock_protocol socket_protocol;
    msock_state socket_state;

    char ip_addr[INET_ADDRSTRLEN];

    void *user_data;
};

struct msock_server {
    SOCKET native_socket;
    msock_protocol socket_protocol;
    msock_state socket_state;

    msock_client connected_clients[MSOCK_MAX_CLIENTS];
    msock_on_connect_cb connect_cb;
    msock_on_disconnect_cb disconnect_cb;
    msock_on_client_cb client_cb;
};

typedef struct {
    char* buffer;
    size_t size;  
    size_t len;
} msock_message;

bool msock_init();
bool msock_deinit();
bool msock_get_local_ip(char *buffer, size_t buffer_len);

void msock_set_nonblocking(SOCKET sock);

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
bool msock_server_run(msock_server *server);

bool msock_server_broadcast(msock_server *server_socket, msock_message *boardcast_msg);

void msock_server_set_connect_cb(msock_server *server_socket, msock_on_connect_cb cb);
void msock_server_set_disconnect_cb(msock_server *server_socket, msock_on_disconnect_cb cb);
void msock_server_set_client_cb(msock_server *server_socket, msock_on_client_cb cb);

#ifdef MSOCK_IMPLEMENTATION

//BASE UTIL

bool msock_init() {
#ifdef _WIN32
    WSADATA wsa_data;

    int success = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if(success != 0) {
        printf("WSAStartup failed: %d\n", success);
        return false;
    }
#endif
    return true;
}

bool msock_deinit() {
#ifdef _WIN32
    WSACleanup();
#endif
    return true;
}

bool msock_get_local_ip(char *buffer, size_t buffer_len) {
    char hostname[256];
    
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        return false;
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;       // We only want IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    struct addrinfo *result = NULL;
    
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        return false;
    }

    struct addrinfo *ptr = NULL;
    bool found = false;

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        struct sockaddr_in *sock_addr = (struct sockaddr_in *)ptr->ai_addr;
        
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sock_addr->sin_addr, ip_str, INET_ADDRSTRLEN);

        if (strcmp(ip_str, "127.0.0.1") != 0) {
            snprintf(buffer, buffer_len, "%s", ip_str);
            found = true;
            break;
        }
    }

    freeaddrinfo(result);
    return found;
}

void msock_set_nonblocking(SOCKET sock) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

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

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* info;
    getaddrinfo(ip, port, &hints, &info);

    bool success = connect(client_socket->native_socket, info->ai_addr, (int)info->ai_addrlen) == 0;
    freeaddrinfo(info);

    if(!success) {
        
    }

    client_socket->socket_state = MSOCK_STATE_CONNECTED;
    return success;
}

bool msock_client_is_connected(msock_client *client_socket) {
    return client_socket->socket_state == MSOCK_STATE_CONNECTED;
}

bool msock_client_close(msock_client *client_socket) {
    bool success = true;

    if(client_socket->socket_state == MSOCK_STATE_DISCONNECTED) return success;
    if(shutdown(client_socket->native_socket, SD_SEND) == SOCKET_ERROR) {
        printf("shutdown() failed: %d\n", WSAGetLastError());
        success = false;
    }

    closesocket(client_socket->native_socket);

    client_socket->socket_state = MSOCK_STATE_DISCONNECTED;

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
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            result_msg->len = 0;
            return true;
        }

        printf("Connection lost: %d\n", err);
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

    memset(server_result->connected_clients, 0, sizeof(server_result->connected_clients));
    for(int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        server_result->connected_clients[i].socket_state = MSOCK_STATE_DISCONNECTED;
    }

    return true;
}

bool msock_server_listen(msock_server *server_socket, const char* ip, const char* port) {
    msock_set_nonblocking(server_socket->native_socket);

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

bool msock_server_close(msock_server *server_socket) {
    bool success = true;

    for(int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        msock_client *client = &server_socket->connected_clients[i];

        if(client->socket_state == MSOCK_STATE_DISCONNECTED) continue;

        if(shutdown(client->native_socket, SD_SEND) == SOCKET_ERROR) {
            printf("shutdown() client %d failed: %d\n", i, WSAGetLastError());
            success = false;
        }

        if (server_socket->disconnect_cb) server_socket->disconnect_cb(client);
        closesocket(client->native_socket);

        client->native_socket = INVALID_SOCKET;
        client->socket_state = MSOCK_STATE_DISCONNECTED;
    }

    closesocket(server_socket->native_socket);

    return success;
}

msock_status msock_server_accept(msock_server *server_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    SOCKET native_client = accept(server_socket->native_socket, (struct sockaddr*)&client_addr, &addr_len);
    
    if (native_client == INVALID_SOCKET) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return MSOCK_NO_WORK; 
        }
        printf("accept() failed: %d\n", error);
        return MSOCK_ERROR;
    }

    int free_slot = -1;
    for (int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        if (server_socket->connected_clients[i].socket_state == MSOCK_STATE_DISCONNECTED) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        printf("Server full! Rejecting connection.\n");
        closesocket(native_client);
        return MSOCK_ERROR;
    }

    msock_client *client = &server_socket->connected_clients[free_slot];
    client->native_socket = native_client;
    client->socket_state = MSOCK_STATE_CONNECTED;

    inet_ntop(AF_INET, &client_addr.sin_addr, client->ip_addr, INET_ADDRSTRLEN);

    return MSOCK_SUCCESS;
}

static void msock_internal_handle_accept(msock_server *server) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    SOCKET new_socket = accept(server->native_socket, (struct sockaddr*)&address, &addrlen);
    
    if (new_socket == INVALID_SOCKET) {
        printf("accept() failed, INVALID_SOCKET. Error: %d\n", WSAGetLastError());
        return;
    }

    int free_slot = -1;
    for(int i=0; i<MSOCK_MAX_CLIENTS; i++) {
        if(server->connected_clients[i].socket_state == MSOCK_STATE_DISCONNECTED) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        printf("Server full, rejecting connection.\n");
        closesocket(new_socket);
        return;
    }

    msock_client *c = &server->connected_clients[free_slot];
    c->native_socket = new_socket;
    c->socket_state = MSOCK_STATE_CONNECTED;
    
    inet_ntop(AF_INET, &address.sin_addr, c->ip_addr, INET_ADDRSTRLEN);
    
    msock_set_nonblocking(new_socket);

    bool allow = true;
    if (server->connect_cb != NULL) {
        allow = server->connect_cb(c);
    }

    if (!allow) {
        closesocket(new_socket);
        c->socket_state = MSOCK_STATE_DISCONNECTED;
    }
}

static void msock_internal_handle_clients(msock_server *server_socket, fd_set *readfds) { 
    for (int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        msock_client *client = &server_socket->connected_clients[i];

        if (client->socket_state == MSOCK_STATE_CONNECTED && 
            FD_ISSET(client->native_socket, readfds)) {
            
            bool keep_alive = true;
            if (server_socket->client_cb != NULL) {
                keep_alive = server_socket->client_cb(server_socket, client);
            }

            if (!keep_alive) {
                msock_client_close(client);
                server_socket->disconnect_cb(client);
                client->socket_state = MSOCK_STATE_DISCONNECTED;
            }
        }
    }
}

bool msock_server_run(msock_server *server) {
    fd_set readfds;
    FD_ZERO(&readfds);

    FD_SET(server->native_socket, &readfds);

int max_fd = 0; 

#ifndef _WIN32
    // 2. Only calculate max_fd on Linux/Mac
    max_fd = server->native_socket;

    for (int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        msock_client *client = &server->connected_clients[i];
        if (client->socket_state == MSOCK_STATE_CONNECTED) {
            FD_SET(client->native_socket, &readfds);
            
            if(client->native_socket > max_fd) {
                max_fd = client->native_socket;
            }
        }
    }
#else
    for (int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        msock_client *client = &server->connected_clients[i];
        if (client->socket_state == MSOCK_STATE_CONNECTED) {
            FD_SET(client->native_socket, &readfds);
        }
    }
#endif

    int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
    if (activity == SOCKET_ERROR) {
        printf("select() error: %d\n", WSAGetLastError());
        return false;
    }

    if (FD_ISSET(server->native_socket, &readfds)) {
        msock_internal_handle_accept(server);
    }

    msock_internal_handle_clients(server, &readfds);

    return true;
}

bool msock_server_broadcast(msock_server *server_socket, msock_message *broadcast_msg) {

    for(int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        if(server_socket->connected_clients[i].socket_state == MSOCK_STATE_DISCONNECTED) continue;
    
        msock_client_send(&server_socket->connected_clients[i], broadcast_msg);
    }


    return true;
}

void msock_server_set_connect_cb(msock_server *server_socket, msock_on_connect_cb cb) {
    server_socket->connect_cb = cb;
}

void msock_server_set_disconnect_cb(msock_server *server_socket, msock_on_disconnect_cb cb) {
    server_socket->disconnect_cb = cb;
}

void msock_server_set_client_cb(msock_server *server_socket, msock_on_client_cb cb) {
    server_socket->client_cb = cb;
}

#endif //MSOCK_IMPLEMTATION
#endif //MSOCK_H