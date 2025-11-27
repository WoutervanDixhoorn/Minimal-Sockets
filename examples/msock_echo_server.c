#include <stdio.h>

#define MSOCK_IMPLEMENTATION
#include "msock.h"

bool handle_connect(msock_client *client) {
    printf("Client connected with IP: %s\n", client->ip_addr);
    
    return true;
}

bool handle_disconnect(msock_client *client) {
    printf("Client disconnected with IP: %s\n", client->ip_addr);
    
    return true;
}

bool handle_client(msock_server *server, msock_client *client) {
    (void) server;

    char receive_buffer[256];
    msock_message receive_msg = { .size = 256, .buffer = receive_buffer };

    if (!msock_client_receive(client, &receive_msg)) {
        printf("Client disconnected or receive failed.\n");
        return false;
    }

    if (receive_msg.len > 0) {
        printf("Received: %s\n", receive_msg.buffer);
        msock_client_send(client, &receive_msg);
    }

    return true;
}

int main(void) {

    msock_init();

    msock_server server;
    msock_server_create(&server);
    if (!msock_server_listen(&server, "127.0.0.1", "420")) {
        printf("Failed to bind port 420\n");
        return 1;
    }

    msock_server_set_connect_cb(&server, handle_connect);
    msock_server_set_disconnect_cb(&server, handle_disconnect);
    msock_server_set_client_cb(&server, handle_client);

    printf("msock server started...\n");

    while(msock_server_is_listening(&server)) {
        msock_server_run(&server);
    }

    msock_server_close(&server);

    msock_deinit();

    return 0;
}