#include <stdio.h>

#define MSOCK_IMPLEMENTATION
#include "msock.h"

void handle_client(msock_server *server) {

    char receive_buffer[1024];
    msock_message receive_msg = {
        .size = 1024,
        .buffer = receive_buffer
    };

    while(1) {
        if (!msock_server_receive(server, &receive_msg)) {
            printf("Client disconnected or receive failed.\n");
            break; 
        }

        printf("Received: %s\n", receive_msg.buffer);

        if(msock_server_send(server, &receive_msg)) {
            printf("Send: %s\n", receive_msg.buffer);
        }
    }

    msock_server_close_client(server);
}

int main(void) {

    msock_server server;
    msock_server_listen(&server, "127.0.0.1", "420");

    while(msock_server_is_listening(&server)) {

        msock_status status = msock_server_accept(&server);
        if(status == MSOCK_ERROR) {
            printf("Error in msock_server_accept()\n");
            break;
        }

        if(status == MSOCK_SUCCESS) {
            printf("Accepting client socket succeeded!\n");

            handle_client(&server); 
        }
    }

    msock_server_close(&server);

    return 0;
}