#include "error.h"
#include "socket_layer.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util.h"
#include <string.h>
#include <stdio.h>

#define MAX_FILE_SIZE 1024
#define FILE_DELIMITER "<EOF>"
#define SIZE_DELIMITER "|"
#define SIZE_BUFFER_LENGTH 32

int main(int argc, char* argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return ERR_INVALID_ARGUMENT;
    }

    uint16_t port_number = atouint16(argv[1]);
    int socket_id = tcp_server_init(port_number);
    if (socket_id < 0) {
        perror("Failed to initialize server");
        return ERR_IO;
    }

    printf("Server started on port: %d\n", port_number);
    fflush(stdout);

    while (1) {
        printf("Waiting for a size...\n");
        fflush(stdout);

        int active_socket = tcp_accept(socket_id);
        if (active_socket != -1) {
            char size_buffer[SIZE_BUFFER_LENGTH] = {0};
            ssize_t bytes_read = tcp_read(active_socket, size_buffer, SIZE_BUFFER_LENGTH);
            if (bytes_read == -1) {
                perror("Failed to read size");
                close(active_socket);
                return ERR_IO;
            }

            char* delim_pos = strstr(size_buffer, SIZE_DELIMITER);
            if (delim_pos == NULL) {
                fprintf(stderr, "Size delimiter not found\n");
                close(active_socket);
                return ERR_IO;
            }

            *delim_pos = '\0';
            int file_size = atoi(size_buffer);
            printf("Received a size: %d --> accepted\n", file_size);
            fflush(stdout);

            if (file_size >= MAX_FILE_SIZE) {
                const char* response = "Big file";
                if (tcp_send(active_socket, response, strlen(response)) == -1) {
                    perror("Failed to send response");
                    close(active_socket);
                    return ERR_IO;
                }
            } else {
                const char* response = "Small file";
                if (tcp_send(active_socket, response, strlen(response)) == -1) {
                    perror("Failed to send response");
                    close(active_socket);
                    return ERR_IO;
                }
            }

            printf("About to receive a file of %d bytes\n", file_size);
            fflush(stdout);

            char* file_buffer = calloc((size_t) file_size + strlen(FILE_DELIMITER) + 1, 1);
            file_size += strlen(FILE_DELIMITER);

            int total_bytes = 0;
            ssize_t nb_bytes = 0;
            while ((delim_pos = strstr(file_buffer, FILE_DELIMITER)) == NULL) {
                nb_bytes = tcp_read(active_socket, file_buffer + total_bytes, (size_t) (file_size - total_bytes));
                if (nb_bytes == -1) {
                    perror("Failed to read file");
                    close(active_socket);
                    free(file_buffer);
                    return ERR_IO;
                }
                total_bytes += (int) nb_bytes;
            }

            *delim_pos = '\0';
            printf("Received a file:\n%s\n", file_buffer);

            const char* acknowledge = "Accepted";
            if (tcp_send(active_socket, acknowledge, strlen(acknowledge)) == -1) {
                perror("Failed to send acknowledgment");
                close(active_socket);
                free(file_buffer);
                return ERR_IO;
            }
            free(file_buffer);
            close(active_socket);
        }
    }
    return 0;
}
