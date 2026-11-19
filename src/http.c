#define _GNU_SOURCE

#include "http.h"
#include <string.h>


static int is_valid_method_char(unsigned char character)
{
    if ((character >= 'A' && character <= 'Z') ||
        (character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9') ||
        character == '-' || character == '_') {
        return 1;
    }

    const char *valid_special_chars = "!#$%&'*+.^`|~";
    
    if (strchr(valid_special_chars, character) != NULL) {
        return 1;
    }

    return 0;
}


static int extract_request_line(
    const char *line,
    size_t line_len,
    struct http_request *request
)
{
    if (line == NULL || request == NULL || line_len == 0) {
        return -1;
    }

    const char *method_end = memchr(line, ' ', line_len);

    if (method_end == NULL) {
        return -1;
    }

    size_t method_len = (size_t)(method_end - line);

    if (method_len == 0 || method_len >= HTTP_METHOD_SIZE) {
        return -1;
    }

    for (size_t i = 0; i < method_len; i++) {
        if (!is_valid_method_char((unsigned char)line[i])) {
            return -1;
        }
    }

    const char *target_start = method_end + 1;

    size_t remaining_len =
        line_len - (size_t)(target_start - line);

    const char *target_end =
        memchr(target_start, ' ', remaining_len);

    if (target_end == NULL) {
        return -1;
    }

    size_t target_len = (size_t)(target_end - target_start);

    if (target_len == 0 || target_len >= HTTP_TARGET_SIZE) {
        return -1;
    }

    const char *version_start = target_end + 1;

    size_t version_len =
        line_len - (size_t)(version_start - line);

    if (version_len == 0 || version_len >= HTTP_VERSION_SIZE) {
        return -1;
    }

    if (memchr(version_start, ' ', version_len) != NULL) {
        return -1;
    }

    if (target_start[0] != '/') {
        return -1;
    }

    if (version_len != 8 ||
        memcmp(version_start, "HTTP/1.1", 8) != 0) {
        return -1;
    }

    memcpy(request->method, line, method_len);
    request->method[method_len] = '\0';

    memcpy(request->target, target_start, target_len);
    request->target[target_len] = '\0';

    memcpy(request->version, version_start, version_len);
    request->version[version_len] = '\0';

    return 0;
}

static int is_valid_header_name_char(unsigned char character){
    if ((character >= 'A' && character <= 'Z') ||
        (character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9') ||
        character == '-' || character == '_') {
        return 1;
    }

    const char *valid_special_chars = "!#$%&'*+.^`|~";

    return strchr(valid_special_chars, character) != NULL;
}

static int parse_header_line(const char *line, size_t line_len){
    if (line == NULL || line_len == 0 || line_len > HTTP_MAX_HEADER_LINE){
        return -1;
    }

    const char *separator = memchr(line, ':', line_len);

    if(separator == NULL){
        return -1;
    }

    size_t name_len = (size_t)(separator - line);

    if(name_len == 0){
        return -1;
    }

    for (size_t i = 0; i < name_len; i++){
        if(!is_valid_header_name_char((unsigned char)line[i])){
            return -1;
        }
    }

    const char *value_start = separator + 1;
    size_t value_len = line_len - name_len - 1;

    for (size_t i = 0; i < value_len; i++){
        unsigned char carac = (unsigned char)value_start[i];

        if ((carac < 0x20 && carac != '\t') || carac == 0x7F){
            return -1;
        }
    }

    return 0;
}

enum http_parse_result parse_http_request(const char *buffer, size_t buffer_len, struct http_request *request, size_t *bytes_consumed){

    if (buffer == NULL || request == NULL || bytes_consumed == NULL){
        return HTTP_BAD_REQUEST;
    }
    
    *bytes_consumed = 0;

    const char *headers_end = memmem(buffer, buffer_len, "\r\n\r\n", 4);

    if (headers_end == NULL){
        return HTTP_INCOMPLETE;
    }

    const char *request_line_end = memmem(buffer, buffer_len, "\r\n", 2);

    if (request_line_end == NULL || request_line_end >= headers_end){
        return HTTP_BAD_REQUEST;
    }

    size_t request_line_len = (size_t)(request_line_end - buffer);

    if (request_line_len > HTTP_MAX_REQUEST_LINE){
        return HTTP_BAD_REQUEST;
    }

    if (extract_request_line(buffer, request_line_len, request) != 0){
        return HTTP_BAD_REQUEST;
    }

    const char *header_start = request_line_end + 2;

    size_t header_len = (size_t)(headers_end - header_start);

    if(header_len > HTTP_MAX_HEADERS_SIZE){
        return HTTP_BAD_REQUEST;
    }

    const char *cursor = header_start;

    while (cursor < headers_end){
        const char *line_end = memchr(cursor, '\r', (size_t)(headers_end - cursor));

        if(line_end == NULL || line_end + 1 >= headers_end + 2 || line_end[1] != '\n'){
            return HTTP_BAD_REQUEST;
        }

        size_t line_len = (size_t)(line_end - cursor);

        if (line_len > HTTP_MAX_HEADER_LINE){
            return HTTP_BAD_REQUEST;
        }

        if (parse_header_line(cursor, line_len) != 0){
            return HTTP_BAD_REQUEST;
        }
        
        cursor = line_end + 2;
    }
    
    return HTTP_INCOMPLETE;

}