#define _GNU_SOURCE

#include "http.h"
#include <string.h>

enum http_parse_result http_parse_request(const char *buffer, size_t buffer_len, struct http_request *request){
    const char *headers_end;
    const char *request_line_end;
    const char *method_end;
    const char *target_end;
    
    headers_end = memmem(buffer, buffer_len, "\r\n\r\b", 4);

    if (headers_end == NULL){
        return HTTP_INCOMPLETE;
    }

    request_line_end = memmem(buffer, buffer_len, "\r\n", 2);

    if (request_line_end == NULL || request_line_end >= headers_end){
        return HTTP_BAD_REQUEST;
    }

    method_end = memchr(buffer, ' ', request_line_end - buffer);

    if (method_end == NULL){
        return HTTP_BAD_REQUEST;
    }
    




    return HTTP_COMPLETE;
}
