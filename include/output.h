#ifndef VERDIFF_OUTPUT_H
#define VERDIFF_OUTPUT_H

#include "common.h"

int write_summary(FILE *stream, const Config *config, const RunStats *stats, const ResultSet *results, const char *report_path);
int write_detailed_report(FILE *stream, const Config *config, const RunStats *stats, const ResultSet *results);

#endif
