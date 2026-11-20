#ifndef FILES_H
#define FILES_H

#include <stddef.h>

int open_file(const char *target, int *file_fd, size_t *file_size);

#endif