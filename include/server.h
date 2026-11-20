#ifndef SERVER_H
#define SERVER_H

#include "http.h"

#include<stddef.h>

#define SIZEBUF (HTTP_MAX_REQUEST_LINE + HTTP_MAX_HEADERS_SIZE + 4)
#define FILE_MAX 8192

enum client_type {
    LISTENER,
    CLIENT
};

struct client {
    enum client_type type;
    int fd;
    char buffer[SIZEBUF];
    size_t buffer_len;
    struct http_request request;
    char response[4096];
    size_t response_len;
    size_t response_sent;
    int file_fd;
    size_t file_size;
    size_t file_offset;
    char file_buffer[FILE_MAX];
    size_t file_buffer_len;
    size_t file_buffer_sent;
    int close_after_response;
    int read_closed;
    struct client *next;
};


int server_init(void);

#endif