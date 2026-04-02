#ifndef VERDIFF_HASH_H
#define VERDIFF_HASH_H

#include "common.h"

int hash_file_xxh3(const char *path, size_t buffer_size, size_t mmap_threshold, uint64_t *hash_out);

#endif
