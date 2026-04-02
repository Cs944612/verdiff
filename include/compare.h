#ifndef VERDIFF_COMPARE_H
#define VERDIFF_COMPARE_H

#include "common.h"
#include "thread_pool.h"

/*
 * compare_candidate_file: The heart of the beast.
 * We throw a worker at this. It checks sizes, hashes contents (bless you XXH3),
 * handles the zero-copy mmap fast-paths, does a strict byte-for-byte check to foil
 * hash collision conspiracies, and extracts line diffs if you asked nicely.
 */
int compare_candidate_file(const CompareContext *context, const char *relative_path);

/*
 * plan_and_compare: The big boss function.
 * Spins up the thread pool, iterates through our hand-rolled hash map indexes
 * of both directories, finds out what was added/removed/modified, and then shoves
 * the rest into the multithreaded grater.
 */
int plan_and_compare(const Config *config, const FileIndex *index_a, const FileIndex *index_b, ResultSet *results, ProgressState *progress);

/*
 * detect_line_differences: Standard line-by-line comparison because calling `diff`
 * in a subprocess is too mainstream. Only runs on things that look like text.
 */
int detect_line_differences(const char *path_a, const char *path_b, LineDiffVector *lines);

#endif
