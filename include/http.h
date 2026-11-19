#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_METHOD_SIZE 16
#define HTTP_TARGET_SIZE 2048
#define HTTP_VERSION_SIZE 16

#define HTTP_MAX_REQUEST_LINE 4096
#define HTTP_MAX_HEADER_LINE 8192
#define HTTP_MAX_HEADERS_SIZE 16384

enum http_parse_result {
    HTTP_INCOMPLETE,
    HTTP_COMPLETE,
    HTTP_BAD_REQUEST
};

struct http_request {
    char method[HTTP_METHOD_SIZE];
    char target[HTTP_TARGET_SIZE];
    char version[HTTP_VERSION_SIZE];
};

enum http_parse_result parse_http_request(
    const char *buffer,
    size_t buffer_len,
    struct http_request *request,
    size_t *bytes_consumed
);

#endif