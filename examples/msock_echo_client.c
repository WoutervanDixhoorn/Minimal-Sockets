#include <stdio.h>
#include <string.h>

#define MSOCK_IMPLEMENTATION
#include "msock.h"

#ifndef _WIN32
    #include <unistd.h>
    #define Sleep(ms) usleep((ms) * 1000)
#endif

int main(void) {
    
    msock_init();

    msock_client client;
    msock_client_create(&client);
    msock_client_connect(&client, "127.0.0.1", "420");

    char receive_buffer[1024];
    msock_message receive_msg = {
        .size = 1024,
        .buffer = receive_buffer
    };

    msock_message msg = {.buffer = "Echo!", .len = 5};
    while(msock_client_is_connected(&client)) {

        if(!msock_client_send(&client, &msg)) break;
        printf("Send: %s\n", msg.buffer);

        if(!msock_client_receive(&client, &receive_msg)) break;
        printf("Received: %s\n", receive_buffer);
        
        Sleep(1000);
    }

    msock_client_close(&client);
    
    msock_deinit();

    return 0;
}