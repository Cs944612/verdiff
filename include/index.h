#ifndef VERDIFF_INDEX_H
#define VERDIFF_INDEX_H

#include "common.h"

/*
 * file_index_init / destroy:
 * Birth and death of our custom path hash map. 'initial_capacity' is basically 
 * a polite suggestion. It will snap up to the next power of 2 because bitwise AND 
 * probing is computationally cheaper than modulo division magic.
 */
int file_index_init(FileIndex *index, size_t initial_capacity);
void file_index_destroy(FileIndex *index);

/*
 * file_index_upsert:
 * Drops a file path into the index. Resolves collisions linearly because CPU cache 
 * lines just love contiguous arrays. If the load factor hits 70%, it forcibly doubles 
 * the universe capacity and redistributes the stars.
 */
int file_index_upsert(FileIndex *index, const char *path, off_t size, time_t mtime);

/*
 * file_index_find / find_const:
 * Give it a path, and it tells you if it exists. Blazing fast. 
 * No disk I/O, pure in-memory goodness.
 */
FileInfo *file_index_find(FileIndex *index, const char *path);
const FileInfo *file_index_find_const(const FileIndex *index, const char *path);

#endif
