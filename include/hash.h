#ifndef VERDIFF_HASH_H
#define VERDIFF_HASH_H

#include "common.h"

/*
 * hash_file_xxh3: 
 * Feeds a given file into the blazing fast non-cryptographic XXH3 64-bit algorithm.
 * Uses `mmap()` for small files to dodge kernel boundary tax, and streaming 
 * blocks for massive files so we don't accidentally OOM your server.
 */
int hash_file_xxh3(const char *path, size_t buffer_size, size_t mmap_threshold, uint64_t *hash_out);

#endif
