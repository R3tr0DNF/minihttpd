
#define _XOPEN_SOURCE 700

#include "files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

int open_file(const char *target, int *fd_file, size_t *file_size){

    char root[PATH_MAX];
    char path[PATH_MAX];
    char resolved[PATH_MAX];

    *fd_file = -1;
    *file_size = 0;

    if (target == NULL || target[0] != '/'){
        return 400;
    }

    if (realpath("www", root) == NULL){
        return 500;
    }

    size_t target_len = strcspn(target, "?#");

    if (target_len == 0){
        return 400;
    }

    if ((target_len == 1 && target[0] == '/') || target[target_len - 1] == '/'){
        if (snprintf(path, sizeof(path), "%s/index.html", root) >= (int)sizeof(path)){
            return 404;
        }
    }else{
        if (snprintf(path, sizeof(path), "%s%.*s", root, (int)target_len, target) >= (int)sizeof(path)){
            return 404;
        }
    }

    if (realpath(path, resolved) == NULL){
        if (errno == EACCES || errno == EPERM){
            return 403;
        }

        if (errno == ENOENT || errno == ENOTDIR){
            return 404;
        }

        return 500;
    }

    size_t root_len = strlen(root);

    if (strncmp(resolved, root, root_len) != 0 || (resolved[root_len] != '/' && resolved[root_len] != '\0')){
        return 403;
    }

    int fd = open(resolved, O_RDONLY | O_CLOEXEC);

    if (fd == -1){
        if (errno == EACCES || errno == EPERM){
            return 403;
        }

        if (errno == ENOENT || errno == ENOTDIR){
            return 404;
        }

        return 500;
    }

    struct stat file_info;

    if (fstat(fd, &file_info) == -1){
        close(fd);
        return 500;
    }

    if (!S_ISREG(file_info.st_mode)){
        close(fd);
        return 403;
    }

    *fd_file = fd;
    *file_size = (size_t)file_info.st_size;

    return 200;
}
