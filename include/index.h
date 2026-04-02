#ifndef VERDIFF_INDEX_H
#define VERDIFF_INDEX_H

#include "common.h"

int file_index_init(FileIndex *index, size_t initial_capacity);
void file_index_destroy(FileIndex *index);
int file_index_upsert(FileIndex *index, const char *path, off_t size, time_t mtime);
FileInfo *file_index_find(FileIndex *index, const char *path);
const FileInfo *file_index_find_const(const FileIndex *index, const char *path);

#endif
