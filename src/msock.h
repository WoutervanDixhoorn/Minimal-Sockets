#ifndef MSOCK_H
#define MSOCK_H

#include <ws2tcpip.h>
#include <winsock.h>

#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define MSOCK_MAX_CLIENTS 64

typedef struct msock_client msock_client;
typedef struct msock_server msock_server;

typedef bool (*msock_on_connect_cb)(msock_client *client);
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
    char ip_addr[INET_ADDRSTRLEN];

    msock_protocol socket_protocol;
    msock_state socket_state;
};

struct msock_server {
    SOCKET native_socket;

    msock_protocol socket_protocol;
    msock_state socket_state;

    //SOCKET native_client_socket;
    msock_client connected_clients[MSOCK_MAX_CLIENTS];
    msock_on_connect_cb connect_cb;
    msock_on_client_cb client_cb;
};

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

void msock_server_set_connect_cb(msock_server *server_socket, msock_on_connect_cb cb);
void msock_server_set_client_cb(msock_server *server_socket, msock_on_client_cb cb);

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
    msock_server_create(server_socket);

    u_long mode = 1;
    ioctlsocket(server_socket->native_socket, FIONBIO, &mode); //Always non blocking for now!

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

        closesocket(client->native_socket);

        client->native_socket = INVALID_SOCKET;
        client->socket_state = MSOCK_STATE_DISCONNECTED;
    }

    closesocket(server_socket->native_socket);

    return success;
}

msock_status msock_server_accept(msock_server *server_socket) {
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);

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

// bool msock_server_close_client(msock_server *server_socket) {
//     SOCKET client_sock = server_socket->native_client_socket;

//     if (client_sock == INVALID_SOCKET) return false;

//     if (shutdown(client_sock, SD_BOTH) == SOCKET_ERROR) {
//         printf("shutdown() failed: %d\n", WSAGetLastError()); 
//         return false;
//     }

//     closesocket(client_sock); 
//     server_socket->native_client_socket = INVALID_SOCKET;

//     return true;
// }

// bool msock_server_send(msock_server *server_socket, msock_message *msg) {
//     if(server_socket->native_client_socket == INVALID_SOCKET) return false;

//     int success = send(server_socket->native_client_socket, msg->buffer, msg->len, 0);
//     if(success == SOCKET_ERROR) {
//         printf("send() failed: %d\n", WSAGetLastError());
//         return false;
//     }

//     return true;    
// }

// bool msock_server_receive(msock_server *server_socket, msock_message *result_msg) {
//     if(server_socket->socket_state != MSOCK_STATE_LISTENING) return false;

//     int bytes_received = recv(server_socket->native_client_socket, result_msg->buffer, result_msg->size - 1, 0); //NOTE: recv is a blocking function

//     if(bytes_received == 0) {
//         printf("Connection closed!\n");
//         return false;
//     }

//     if(bytes_received < 0) {
//         printf("Connection lost: %d\n", WSAGetLastError());
//         return false;
//     }
    
//     result_msg->buffer[bytes_received] = '\0';
//     result_msg->len = bytes_received;
//     return true;
// }

bool msock_server_run(msock_server *server) {
    fd_set readfds;
    FD_ZERO(&readfds);

    FD_SET(server->native_socket, &readfds);

    for (int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        msock_client *client = &server->connected_clients[i];
        if (client->socket_state == MSOCK_STATE_CONNECTED) {
            FD_SET(client->native_socket, &readfds);
        }
    }

    int activity = select(0, &readfds, NULL, NULL, NULL); //Timout NULL = wait forever

    if (activity == SOCKET_ERROR) {
        printf("select() error: %d\n", WSAGetLastError());
        return false;
    }

    if (FD_ISSET(server->native_socket, &readfds)) {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        SOCKET new_socket = accept(server->native_socket, (struct sockaddr*)&address, &addrlen);

        if (new_socket != INVALID_SOCKET) {
            int free_slot = -1;
            for(int i=0; i<MSOCK_MAX_CLIENTS; i++) {
                if(server->connected_clients[i].socket_state == MSOCK_STATE_DISCONNECTED) {
                    free_slot = i;
                    break;
                }
            }

            if (free_slot != -1) {
                msock_client *c = &server->connected_clients[free_slot];
                c->native_socket = new_socket;
                c->socket_state = MSOCK_STATE_CONNECTED;
                inet_ntop(AF_INET, &address.sin_addr, c->ip_addr, INET_ADDRSTRLEN);
                
                u_long mode = 1;
                ioctlsocket(new_socket, FIONBIO, &mode);

                // A3. Call the User's "On Connect" Callback
                bool allow_connection = true;
                if (server->connect_cb != NULL) {
                    allow_connection = server->connect_cb(c);
                }

                if (!allow_connection) {
                    // User rejected (IP Ban, etc.)
                    closesocket(new_socket);
                    c->socket_state = MSOCK_STATE_DISCONNECTED;
                } 
            } else {
                // Server Full
                printf("Server full, rejecting connection.\n");
                closesocket(new_socket);
            }
        }
    }

    for (int i = 0; i < MSOCK_MAX_CLIENTS; i++) {
        msock_client *client = &server->connected_clients[i];

        if (client->socket_state == MSOCK_STATE_CONNECTED) {
            if (FD_ISSET(client->native_socket, &readfds)) {
                
                bool keep_alive = true;
                if (server->client_cb != NULL) {
                    keep_alive = server->client_cb(server, client);
                }

                if (!keep_alive) {
                    msock_client_close(client);
                    client->socket_state = MSOCK_STATE_DISCONNECTED;
                }
            }
        }
    }

    return true;
}

void msock_server_set_connect_cb(msock_server *server_socket, msock_on_connect_cb cb) {
    server_socket->connect_cb = cb;
}

void msock_server_set_client_cb(msock_server *server_socket, msock_on_client_cb cb) {
    server_socket->client_cb = cb;
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

#endif //MSOCK_IMPLEMTATION
#endif //MSOCK_H