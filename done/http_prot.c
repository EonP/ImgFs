#include <stdio.h>
#include <stdlib.h>
#include "http_prot.h"
#include <string.h>
#include "error.h"
#include "util.h"

#define WHITESPACE " "
#define AMPERSAND '&'
#define CONTENT_LEN_STR "Content-Length"

int http_match_uri(const struct http_message *message, const char *target_uri)
{
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(target_uri);

    size_t uri_length = strlen(target_uri);

    return strncmp(message->uri.val, target_uri, uri_length) == 0 ? 1 : 0;
}

int http_match_verb(const struct http_string* method, const char* verb)
{
    M_REQUIRE_NON_NULL(method);
    M_REQUIRE_NON_NULL(verb);

    size_t verb_length = strlen(verb);

    if (verb_length != method->len) return 0;

    return strncmp(method->val, verb, verb_length) == 0 ? 1 : 0;
}

int http_get_var(const struct http_string* url, const char* name, char* out, size_t out_len)
{
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(out);
    if (out_len <= 0) return ERR_INVALID_ARGUMENT;

    size_t name_len = strlen(name);

    // copy of name parameter (to contain =name)
    char* name_copy = calloc(1, name_len + 2);
    if (name_copy == NULL) return ERR_OUT_OF_MEMORY;

    // Correctly set name_copy to =name
    if (snprintf(name_copy, name_len + 2, "%s=", name) < 0) return ERR_IO;

    // Allocate memory for url with terminator at end
    char* url_with_terminator = calloc(1, url->len + 1);
    if (url_with_terminator == NULL) {
        free(name_copy); name_copy = NULL;
        return ERR_OUT_OF_MEMORY;
    }

    // Copy contents of url with no terminator to url_with_terminator
    memcpy(url_with_terminator, url->val, url->len);

    char *name_in_url = strstr(url_with_terminator, name_copy);

    free(name_copy); name_copy = NULL; // Not needed anymore

    // if name parameter not found in URL return 0
    if (name_in_url == NULL) {
        free(url_with_terminator); url_with_terminator = NULL;
        return 0;
    }

    // start position of the output in the url
    char* start_pos = name_in_url + name_len + 1;

    // look for & character in url
    char *end_pos = strchr(start_pos, AMPERSAND);

    // If '&' character does not exist, change the end position
    if (end_pos == NULL) {
        end_pos = url_with_terminator + url->len;
    }

    // Length of the output
    size_t final_len = (size_t) (end_pos - start_pos);

    if (final_len > out_len) {
        free(url_with_terminator); url_with_terminator = NULL;
        return ERR_RUNTIME;
    }

    memcpy(out, start_pos, final_len);
    free(url_with_terminator); url_with_terminator = NULL;
    // Return the length of the output
    return (int) final_len;
}

static const char* get_next_token(const char* message, const char* delimiter, struct http_string* output)
{
    if (output == NULL || delimiter == NULL || message == NULL) return NULL;

    // Pointer to the start of the delimeter
    char* delim_start = strstr(message, delimiter);

    if (delim_start == NULL) return NULL;

    // Set output length to the number of characters between delimiter start and start of message
    output->val = message;
    output->len = (size_t) (delim_start - message);

    // return pointer to the string after the delimeter
    return delim_start + strlen(delimiter);
}

static const char* http_parse_headers(const char* header_start, struct http_message* output)
{
    if (header_start == NULL || output == NULL) return NULL;
    size_t line_delim_len = strlen(HTTP_LINE_DELIM);
    struct http_string key;
    struct http_string value;

    // Iterate while the number of headers is less than the max and we haven't reach the end of the header
    while (output->num_headers <= MAX_HEADERS && strncmp(header_start, HTTP_LINE_DELIM, line_delim_len) != 0) {
        zero_init_var(key);
        zero_init_var(value);

        // Fetch the key
        const char* ptr_after_delim1 = get_next_token(header_start, HTTP_HDR_KV_DELIM, &key);
        output->headers[output->num_headers].key = key;

        // Fetch the value corresponding to the key
        const char* ptr_after_delim2 = get_next_token(ptr_after_delim1, HTTP_LINE_DELIM, &value);
        output->headers[output->num_headers].value = value;

        // Increase the number of headers and prepare for the next iteration by moving the header pointer
        ++output->num_headers;
        header_start = ptr_after_delim2;
    }

    // Return position where the body starts
    return header_start + line_delim_len;
}

int http_parse_message(const char *stream, size_t bytes_received, struct http_message *out, int *content_len)
{
    M_REQUIRE_NON_NULL(stream);
    M_REQUIRE_NON_NULL(out);
    M_REQUIRE_NON_NULL(content_len);

    struct http_string third_token;
    zero_init_var(third_token);
    zero_init_ptr(out);

    // Message could not be fully parsed as header is not yet received
    if (strstr(stream, HTTP_HDR_END_DELIM) == NULL) return 0;

    const char* p1 = get_next_token(stream, WHITESPACE, &out->method);
    if (p1 == NULL) return ERR_IO;

    const char* p2 = get_next_token(p1, WHITESPACE, &out->uri);
    if (p2 == NULL) return ERR_IO;

    const char* p3 = get_next_token(p2, HTTP_LINE_DELIM, &third_token);
    if (p3 == NULL) return ERR_IO;

    // If last token != HTTP/1.1, message is incorrect and return ERROR
    if (strncmp("HTTP/1.1", third_token.val, third_token.len) != 0) return -1;

    const char* body_start = http_parse_headers(p3, out);
    if (body_start == NULL) return ERR_IO; // DONT KNOW THE ERR

    size_t found = 0;
    *content_len = 0;

    for (size_t i = 0; i < out->num_headers && !found; ++i) {
        if (strncmp(CONTENT_LEN_STR, out->headers[i].key.val, out->headers[i].key.len) == 0) {
            *content_len = atoi(out->headers[i].value.val);
            found = 1;
        }
    }

    size_t header_len = (size_t) (body_start - stream);
    // Message could not be fully parsed as body is not fully received
    if (header_len + (size_t) *content_len != bytes_received) return 0; // SUS
    // If content length is zero, no bytes
    if (*content_len == 0) return 1;

    // Store the body
    out->body.val = body_start;
    out->body.len = (size_t) *content_len;

    return 1;
}


