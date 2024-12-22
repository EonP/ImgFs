/*
 * @file imgfs_server_services.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t

#include "error.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"

#include <vips/vips.h>

#include <pthread.h>

// Main in-memory structure for imgFS
static struct imgfs_file imgfs_file;
static uint16_t server_port;
static pthread_mutex_t mutex;

#define URI_ROOT "/imgfs"

#define MAX_RES_STR_SIZE 9

/**********************************************************************
 * Sends error message.
 ********************************************************************** */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/**********************************************************************
 * Sends 302 OK message.
 ********************************************************************** */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

/********************************************************************//**
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionnaly port number as argv[2]
 ********************************************************************** */
int server_startup (int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc < 2) return ERR_NOT_ENOUGH_ARGUMENTS;

    // Initialize vips library
    if (VIPS_INIT(argv[0])) return ERR_IMGLIB;

    char* filename = argv[1];
    M_REQUIRE_NON_NULL(filename);

    // Open file in read and write mode
    int err = do_open(filename, "rb+", &imgfs_file);
    if (err) return err;

    print_header(&imgfs_file.header); fflush(stdout);

    // Check if port number is given
    uint16_t port = (argc == 3) ? atouint16(argv[2]) : DEFAULT_LISTENING_PORT;
    err = http_init(port, handle_http_message);

    // If initialization with the given port fails, try the default port
    if (err < 0 && port == DEFAULT_LISTENING_PORT) return err;

    if (err < 0 && port != DEFAULT_LISTENING_PORT) {
        port = DEFAULT_LISTENING_PORT;
        err = http_init(port, handle_http_message);
        if (err < 0) return err;
    }
    printf("ImgFS server started on http://localhost:%u\n", port); fflush(stdout);
    server_port = port;

    // Initialize mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) return ERR_THREADING;

    return ERR_NONE;
}


/********************************************************************//**
 * Shutdown function. Free the structures and close the file.
 ********************************************************************** */
void server_shutdown (void)
{
    fprintf(stderr, "Shutting down...\n");
    vips_shutdown();
    http_close();
    do_close(&imgfs_file);
    pthread_mutex_destroy(&mutex);
}

/**********************************************************************
 * Handles a list request
 ********************************************************************** */
int handle_list_call(int connection)
{
    char* joutput = NULL;
    int ret;

    // Prepare the json format of the imgfs file
    if (pthread_mutex_lock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);
    ret = do_list(&imgfs_file, JSON, &joutput);
    if (pthread_mutex_unlock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);

    // If do_list reply an error
    if(ret != ERR_NONE) {
        if (joutput != NULL) free(joutput); joutput = NULL;
        return reply_error_msg(connection, ret);
    }

    // Reply the json format of the file
    ret = http_reply(connection, HTTP_OK, "Content-Type: application/json\r\n", joutput, strlen(joutput));
    free(joutput); joutput = NULL;
    return ret < 0 ? reply_error_msg(connection, ret) : ERR_NONE;
}

/**********************************************************************
 * Handles a read request
 ********************************************************************** */
int handle_read_call(struct http_message* msg, int connection)
{
    // There are only 3 possible resolutions choose the biggest one for the size of the table
    int ret;

    // Get the requested to read in
    char res_str[MAX_RES_STR_SIZE + 1] = { 0 };
    ret = http_get_var(&msg->uri, "res", res_str, MAX_RES_STR_SIZE);
    if (ret <= 0) return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);

    // Get the image id
    char img_id[MAX_IMG_ID + 1] = { 0 };
    ret = http_get_var(&msg->uri, "img_id", img_id, MAX_IMG_ID);
    if (ret <= 0) return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);

    // Converting res string to an int
    int resolution = resolution_atoi(res_str);
    if (resolution == -1) return reply_error_msg(connection, ERR_RESOLUTIONS);

    // Create a buffer to send image
    char* image_buffer = NULL;
    uint32_t image_size = 0;

    // Read the image
    if (pthread_mutex_lock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);
    int err = do_read(img_id, resolution, &image_buffer, &image_size, &imgfs_file);
    if (pthread_mutex_unlock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);

    // If read fails reply an error
    if (err != ERR_NONE) {
        free(image_buffer); image_buffer = NULL;
        return reply_error_msg(connection, err);
    }

    // Reply the requested image
    ret = http_reply(connection, HTTP_OK, "Content-Type: image/jpeg\r\n", image_buffer, image_size);
    free(image_buffer); image_buffer = NULL;
    return ret < 0 ? reply_error_msg(connection, ret) : ERR_NONE;
}

/**********************************************************************
 * Handles a delete request
 ********************************************************************** */
int handle_delete_call(struct http_message* msg, int connection)
{
    // Get the image id of image to be deleted
    char img_id[MAX_IMG_ID + 1] = { 0 };
    int ret = http_get_var(&msg->uri, "img_id", img_id, MAX_IMG_ID);
    if (ret <= 0) return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);

    // Delete the image
    if (pthread_mutex_lock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);
    ret = do_delete(img_id, &imgfs_file);
    if (pthread_mutex_unlock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);

    // If deletion failed reply an error
    if (ret != ERR_NONE) return reply_error_msg(connection, ret);

    // Reply ok message if deleted image successfully
    ret = reply_302_msg(connection);
    return ret < 0 ? reply_error_msg(connection, ret) : ERR_NONE;
}

/**********************************************************************
 * Handles an insert request
 ********************************************************************** */
int handle_insert_call(struct http_message* msg, int connection)
{
    // Gen the name of the image to insert
    char name[MAX_IMGFS_NAME + 1] = { 0 };
    int ret = http_get_var(&msg->uri, "name", name, MAX_IMGFS_NAME);
    if (ret <= 0) return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);

    // Insert the image in the database
    if (pthread_mutex_lock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);
    ret = do_insert(msg->body.val, msg->body.len, name, &imgfs_file);
    if (pthread_mutex_unlock(&mutex) != ERR_NONE) return reply_error_msg(connection, ERR_THREADING);

    // If inserting failed, reply an error
    if (ret != ERR_NONE) return reply_error_msg(connection, ret);

    // Reply ok message if inserted image successfully in database
    ret = reply_302_msg(connection);
    return ret < 0 ? reply_error_msg(connection, ret) : ERR_NONE;
}

/**********************************************************************
 * Simple handling of http message. TO BE UPDATED WEEK 13
 ********************************************************************** */
int handle_http_message(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);

    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(connection, BASE_FILE);
    }

    debug_printf("handle_http_message() on connection %d. URI: %.*s\n", connection, (int) msg->uri.len, msg->uri.val);

    if (http_match_uri(msg, URI_ROOT "/list")) {
        return handle_list_call(connection);
    }
    if (http_match_uri(msg, URI_ROOT "/insert") && http_match_verb(&msg->method, "POST")) {
        return handle_insert_call(msg, connection);
    }
    if(http_match_uri(msg, URI_ROOT "/read")) {
        return handle_read_call(msg, connection);
    }
    if(http_match_uri(msg, URI_ROOT "/delete")) {
        return handle_delete_call(msg, connection);
    }

    return reply_error_msg(connection, ERR_INVALID_COMMAND);
}

