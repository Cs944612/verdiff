#ifndef VERDIFF_COMPARE_H
#define VERDIFF_COMPARE_H

#include "common.h"
#include "thread_pool.h"

int compare_candidate_file(const CompareContext *context, const char *relative_path);
int plan_and_compare(const Config *config, const FileIndex *index_a, const FileIndex *index_b, ResultSet *results, ProgressState *progress);
int detect_line_differences(const char *path_a, const char *path_b, LineDiffVector *lines);

#endif
