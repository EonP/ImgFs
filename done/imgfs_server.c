/*
 * @file imgfs_server.c
 * @brief ImgFS server part, main
 *
 * @author Konstantinos Prasopoulos
 */

#include "util.h"
#include "imgfs.h"
#include "socket_layer.h"
#include "http_net.h"
#include "imgfs_server_service.h"

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h> // abort()

/********************************************************************/
static void signal_handler(int sig_num _unused)
{
    server_shutdown();
    exit(0);
}

/********************************************************************/
static void set_signal_handler(void)
{
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset() in set_signal_handler()");
        abort();
    }
    action.sa_handler = signal_handler;
    action.sa_flags   = 0;
    if ((sigaction(SIGINT,  &action, NULL) < 0) ||
        (sigaction(SIGTERM,  &action, NULL) < 0)) {
        perror("sigaction() in set_signal_handler()");
        abort();
    }
}

/********************************************************************/

int main (int argc, char *argv[])
{
    int err;
    // Start server
    if ((err = server_startup(argc, argv)) < 0) return err;

    set_signal_handler();

    // Receive while no error
    while ((err = http_receive()) == ERR_NONE);

    server_shutdown();

    return ERR_NONE;
}
