#ifndef VERDIFF_SCANNER_H
#define VERDIFF_SCANNER_H

#include "common.h"
#include "index.h"
#include "progress.h"

/*
 * scan_directory:
 * The tireless worker that crawls your sprawling mono-repos.
 * Instead of recursing and historically exploding your 'C' call stack, we use a 
 * heap-allocated explicit stack frame array. It populates the FileIndex iteratively 
 * so the planner doesn't have to wait on unpredictable disk spinning.
 */
int scan_directory(const char *root, FileIndex *index, ProgressState *progress, ProgressPhase phase);

#endif
