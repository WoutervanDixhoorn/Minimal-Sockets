#include <stdio.h>
#include <string.h>

#define MSOCK_IMPLEMENTATION
#include "msock.h"

int main(void) {
    
    msock_client client;
    msock_client_connect(&client, "127.0.0.1", "420");

    char receive_buffer[1024];
    msock_message receive_msg = {
        .size = 1024,
        .buffer = receive_buffer
    };

    msock_message msg = {.buffer = "Echo!", .len = 5};
    while(msock_client_is_connected(&client)) {

        if(msock_client_send(&client, &msg)) {
            printf("Send: %s\n", msg.buffer);
        }
        
        if(msock_client_receive(&client, &receive_msg)) {
            printf("Received: %s\n", receive_buffer);
        }
        
        Sleep(1000);
    }

    msock_client_close(&client);

    return 0;
}