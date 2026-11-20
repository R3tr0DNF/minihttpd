#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_METHOD_SIZE 16
#define HTTP_TARGET_SIZE 2048
#define HTTP_VERSION_SIZE 16

#define HTTP_MAX_REQUEST_LINE 4096
#define HTTP_MAX_HEADER_LINE 8192
#define HTTP_MAX_HEADERS_SIZE 16384

enum http_status{
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500
};

enum http_parse_result {
    HTTP_INCOMPLETE,
    HTTP_COMPLETE,
    HTTP_BAD_REQUEST
};

struct http_request {
    char method[HTTP_METHOD_SIZE];
    char target[HTTP_TARGET_SIZE];
    char version[HTTP_VERSION_SIZE];

    int keep_alive;
};

enum http_parse_result parse_http_request(
    const char *buffer,
    size_t buffer_len,
    struct http_request *request,
    size_t *bytes_consumed
);

int build_http_response(
    int status_code, 
    const char *content_type, 
    size_t content_length, 
    int keep_alive, 
    char *buffer, 
    size_t buffer_size
);


#endif