#define NOB_IMPLEMENTATION
#include "nob.h"

#define INPUT_FOLDER "examples/"
#define OUTPUT_FOLDER "build/"
#define EXE ".exe"

int compile_socket_program_windows(char* input_file, char* output_file, bool debug) {
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    if(debug)
        nob_cmd_append(&cmd, "-g", "-O0");
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-Isrc/");
    nob_cmd_append(&cmd, "-luser32", "-lWs2_32");
    nob_cmd_append(&cmd, "-o", output_file);
    nob_cc_inputs(&cmd, input_file);

    if(nob_cmd_run_sync(cmd)) return 0;

    return 1;
}

int compile_socket_program_linux(char* input_file, char* output_file, bool debug) {
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    if(debug)
        nob_cmd_append(&cmd, "-gdb", "-O0");
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-Isrc/");
    nob_cmd_append(&cmd, "-lpthread");
    nob_cmd_append(&cmd, "-o", output_file);
    nob_cc_inputs(&cmd, input_file);

    if(nob_cmd_run_sync(cmd)) return 0;

    return 1;
}

int compile_socket_program(char* input_file, char* output_file, bool debug) {
#ifdef _WIN32
compile_socket_program_windows(input_file, output_file, debug);
#else
compile_socket_program_linux(input_file, output_file, debug);
#endif
}

int main(int argc, char** argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool debug = false;
    if(argc > 1 && strcmp(argv[1], "-d") == 0) {
        nob_log(NOB_INFO, "Building with debug symbols");
        debug = true;
    }
    
    #ifdef _WIN32
    if(compile_socket_program_windows(INPUT_FOLDER"msock_echo_client.c", OUTPUT_FOLDER"msock_echo_client.exe", debug) != 0) printf("Failed building echo client\n");
    if(compile_socket_program_windows(INPUT_FOLDER"msock_echo_server.c", OUTPUT_FOLDER"msock_echo_server.exe", debug) != 0) printf("Failed building echo server\n");
    #else
    if(compile_socket_program_linux(INPUT_FOLDER"msock_echo_client.c", OUTPUT_FOLDER"msock_echo_client", debug) != 0) printf("Failed building echo client\n");
    if(compile_socket_program_linux(INPUT_FOLDER"msock_echo_server.c", OUTPUT_FOLDER"msock_echo_server", debug) != 0) printf("Failed building echo server\n");
    #endif

    return 0;
}