#ifndef VERDIFF_SCANNER_H
#define VERDIFF_SCANNER_H

#include "common.h"
#include "index.h"
#include "progress.h"

int scan_directory(const char *root, FileIndex *index, ProgressState *progress, ProgressPhase phase);

#endif
