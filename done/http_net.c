/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"
#include "util.h"

#include <pthread.h>

static int passive_socket = -1;
static EventCallback event_callback;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);

#define CONTENT_LEN_MAX_STRLEN 32

/*******************************************************************
 * Handle connection
 */
static void *handle_connection(void *arg)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    // Check if the argument is null
    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;
    int* socket_id = (int*) arg;

    // Check if callback is null
    if (event_callback == NULL) {
        close(*socket_id);
        free(socket_id);
        return &our_ERR_INVALID_ARGUMENT;
    }

    // Allocate memmory for the receive buffer (enough for the header)
    char* rcvbuf = calloc(MAX_HEADER_SIZE, 1);
    if (rcvbuf == NULL) {
        close(*socket_id);
        free(socket_id);
        return &our_ERR_OUT_OF_MEMORY;
    }

    ssize_t curr_bytes_received = 0;
    size_t total_bytes_received = 0;
    size_t buff_len_allowed = MAX_HEADER_SIZE;
    int done_parsing = 0;
    int extended = 0;
    struct http_message out;
    zero_init_var(out);
    int content_len = 0;

    // Continue receiving bytes while the message hasn't been fully parsed
    while(done_parsing != 1) {
        // Read pack of bytes and store them at the correct offset in the buffer
        curr_bytes_received = tcp_read(*socket_id, rcvbuf + total_bytes_received, buff_len_allowed);

        // If the number of bytes received is negative or null, exit
        if (curr_bytes_received <= 0) {
            free(rcvbuf);
            close(*socket_id);
            free(socket_id);
            return curr_bytes_received == 0 ? &our_ERR_NONE : &our_ERR_IO;
        }

        // Increment buffer offset so the following read is stored at correct position
        total_bytes_received += (size_t) curr_bytes_received;
        // Parse the whole buffer again but with changed total_bytes_received
        done_parsing = http_parse_message(rcvbuf, total_bytes_received, &out, &content_len);

        // If parsing returns an error, exit
        if (done_parsing < 0) {
            free(rcvbuf); rcvbuf = NULL;
            close(*socket_id);
            free(socket_id);
            return &our_ERR_IO;
        }

        // If we have not yet extended the buffer, and that the content length is greater than zero, we extend it once
        if (done_parsing == 0 && !extended && content_len > 0) {
            // Increase buffer length allowed
            buff_len_allowed += (size_t) content_len;
            // Reallocate the buffer to a bigger memory area
            char *temp = realloc(rcvbuf, buff_len_allowed);
            if (temp == NULL) {
                free(rcvbuf);
                close(*socket_id);
                free(socket_id);
                return &our_ERR_OUT_OF_MEMORY;
            }
            rcvbuf = temp;
            extended = 1; // set extended to 1 so that we dont extend again
        }
    }

    // We are done parsing so we can call the callback
    int err = event_callback(&out, *socket_id);
    // Close everything to prepare for a new round
    free(rcvbuf);
    close(*socket_id);
    free(socket_id);
    return err < 0 ? &our_ERR_IO : &our_ERR_NONE;
}


/*******************************************************************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    event_callback = callback;
    return passive_socket;
}

/*******************************************************************
 * Close connection
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            perror("close() in http_close()");
        else
            passive_socket = -1;
    }
}

/*******************************************************************
 * Receive content
 */
int http_receive(void)
{
    pthread_attr_t attr;
    pthread_t thread;

    // Store the active socket on the heap
    int* active_socket = calloc(1, sizeof(int));
    if (active_socket == NULL) return ERR_OUT_OF_MEMORY;

    // Accept the TCP connection
    *active_socket = tcp_accept(passive_socket);
    if (*active_socket == -1) {
        free(active_socket);
        return ERR_IO;
    }

    // Initialize attribute with default values
    if (pthread_attr_init(&attr) != ERR_NONE) {
        close(*active_socket);
        free(active_socket);
        return ERR_THREADING;
    }

    // Set the detach state to the attribute
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != ERR_NONE) {
        close(*active_socket);
        free(active_socket);
        pthread_attr_destroy(&attr);
        return ERR_THREADING;
    }

    // Create a thread
    if (pthread_create(&thread, &attr, handle_connection, (void *)active_socket) != ERR_NONE) {
        close(*active_socket);
        free(active_socket);
        pthread_attr_destroy(&attr);
        return ERR_THREADING;
    }

    // Destroy the attribute object as it's no longer needed
    pthread_attr_destroy(&attr);

    return ERR_NONE;

}

/*******************************************************************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = (size_t) pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}

/*******************************************************************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers, const char *body, size_t body_len)
{
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);

    // Create string containing the content length
    char content_length_str[CONTENT_LEN_MAX_STRLEN + 1];
    int content_length_str_len = snprintf(content_length_str, sizeof(content_length_str), "Content-Length: %zu", body_len);
    // Return error if snprintf fails
    if (content_length_str_len < 0) return ERR_IO;

    // Total length of the message
    size_t total_length =
    strlen(HTTP_PROTOCOL_ID) +
    strlen(status) +
    strlen(HTTP_LINE_DELIM) +
    strlen(headers) +
    (size_t) content_length_str_len +
    strlen(HTTP_HDR_END_DELIM) +
    body_len +
    1; // For the null terminator

    // Allocate memory for the buffer
    char* buffer = calloc(1, total_length);
    if (buffer == NULL) return ERR_IO;

    // Fill the header in the correct format
    int header_length =
    snprintf(
    buffer, total_length, "%s%s%s%s%s%s",
    HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM, headers, content_length_str, HTTP_HDR_END_DELIM
    );

    // Return an err if snprintf fails
    if (header_length < 0) {
        free(buffer);
        return ERR_IO;
    }

    // If the body is non-empty, append it to the end of the header
    if (body_len > 0) {
        memcpy(buffer + header_length, body, body_len);
    }

    // Send everything to the socket
    int ret = (int) tcp_send(connection, buffer, (size_t) header_length + body_len);
    free(buffer); buffer = NULL;
    // Returns the number of bytes sent
    return ret;
}
