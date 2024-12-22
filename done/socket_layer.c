#include "socket_layer.h"
#include "error.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "util.h"
#include <string.h>

// Constant used for the TCP protocol
#define TCP 0
// maximum length for the queue of pending connections
#define BACKLOG 16

int tcp_server_init(uint16_t port)
{
    // Create TCP socket
    int socket_id = socket(AF_INET, SOCK_STREAM, TCP);
    if (socket_id == -1) {
        perror("Error creating TCP socket"); // What do we print
        return ERR_IO;
    }

    int opt = 1;
    if (setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(socket_id);
        return ERR_IO;
    }

    // Create server address
    struct sockaddr_in socket_addr;
    zero_init_var(socket_addr);

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socket_addr.sin_port = htons(port);

    // Bind the socket to the server address
    if (bind(socket_id, (struct sockaddr *) &socket_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("Error when binding");
        close(socket_id);
        return ERR_IO;
    }

    // Start listening for incoming connections
    if (listen(socket_id, BACKLOG) == -1) {
        perror("Error in listen");
        close(socket_id);
        return ERR_IO;
    }

    // Return the socket id
    return socket_id;
}

int tcp_accept(int passive_socket)
{
    return accept(passive_socket, NULL, NULL);
}

ssize_t tcp_read(int active_socket, char* buf, size_t buflen)
{
    M_REQUIRE_NON_NULL(buf);
    return recv(active_socket, buf, buflen, 0);
}

ssize_t tcp_send(int active_socket, const char* response, size_t response_len)
{
    M_REQUIRE_NON_NULL(response);
    return send(active_socket, response, response_len, 0);
}
