#include "error.h"
#include "socket_layer.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

#define MAX_FILE_SIZE 2048
#define MAX_SIZE_BUFFER_SIZE 32
#define TCP 0

#define FILE_DELIMITER "<EOF>"
#define SIZE_DELIMITER "|"

int main(int argc, char* argv[])
{
    if (argc != 3) return ERR_INVALID_ARGUMENT;

    uint16_t port_number = atouint16(argv[1]);
    char* filename = argv[2];
    M_REQUIRE_NON_NULL(filename);

    // Open file in "read binary mode"
    FILE* file = fopen(filename, "rb");
    if (file == NULL) return ERR_IO;

    // Move file pointer to the end of the file
    if (fseek(file, 0, SEEK_END) == -1) {
        fclose(file);
        return ERR_IO;
    }

    // Extract the size of the file in bytes
    long file_size = ftell(file);
    if (file_size == -1) {
        fclose(file);
        return ERR_IO;
    }

    // Check that the file_size is less than 2048
    if (file_size > MAX_FILE_SIZE) {
        fclose(file);
        return ERR_OUT_OF_MEMORY;
    }

    // Move the file pointer back to the start of the file
    if (fseek(file, 0, SEEK_SET) == -1) {
        fclose(file);
        return ERR_IO;
    }

    // Initialize a buffer that will store the contents of the file
    char file_buffer[MAX_FILE_SIZE + 1] = {0};

    // Read the contents of the file and place them in the buffer
    if (fread(file_buffer, 1, (size_t) file_size, file) != (size_t) file_size) {
        fclose(file);
        return ERR_IO;
    }

    // We can close the file as we no longer need it
    fclose(file);

    // Create TCP socket
    int socket_id = socket(AF_INET, SOCK_STREAM, TCP);
    if (socket_id == -1) return ERR_IO;

    struct sockaddr_in socket_addr = {0};
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socket_addr.sin_port = htons(port_number);

    if (connect(socket_id, (struct sockaddr *)&socket_addr, sizeof(struct sockaddr_in)) == -1) {
        close(socket_id);
        return ERR_IO;
    }

    printf("Talking to port %d\n", port_number);
    fflush(stdout);

    char size_buffer[MAX_SIZE_BUFFER_SIZE] = {0};
    snprintf(size_buffer, MAX_SIZE_BUFFER_SIZE, "%ld%s", file_size, SIZE_DELIMITER);

    printf("Sending size: %ld\n", file_size);
    fflush(stdout);
    if (tcp_send(socket_id, size_buffer, strlen(size_buffer)) == -1) {
        close(socket_id);
        return ERR_IO;
    }

    char server_response[MAX_SIZE_BUFFER_SIZE] = {0};
    if (tcp_read(socket_id, server_response, sizeof(server_response)) == -1) {
        close(socket_id);
        return ERR_IO;
    }

    printf("Server responded: %s\n", server_response);
    fflush(stdout);

    printf("Sending %s\n", filename);
    fflush(stdout);

    if (tcp_send(socket_id, file_buffer, (size_t)file_size) == -1) {
        close(socket_id);
        return ERR_IO;
    }

    if (tcp_send(socket_id, FILE_DELIMITER, strlen(FILE_DELIMITER)) == -1) {
        close(socket_id);
        return ERR_IO;
    }

    char ack[9] = {0};
    if (tcp_read(socket_id, ack, sizeof(ack) - 1) == -1) {
        close(socket_id);
        return ERR_IO;
    }

    printf("%s\nDone\n", ack);
    fflush(stdout);

    close(socket_id);

    return ERR_NONE;
}
