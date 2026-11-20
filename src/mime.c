#include "mime.h"

#include <string.h>

const char *get_mime_type(const char *path){
    const char *extension = strrchr(path, '.');

    if (extension == NULL){
        return "application/octet-stream";
    }

    if (strcmp(extension, ".html") == 0){
        return "text/html";
    }

    if (strcmp(extension, ".css") == 0){
        return "text/css";
    }

    if (strcmp(extension, ".js") == 0){
        return "application/javascript";
    }

    if (strcmp(extension, ".png") == 0){
        return "image/png";
    }

    if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0){
        return "image/jpeg";
    }

    return "application/octet-stream";
}