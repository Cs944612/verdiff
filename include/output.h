#ifndef VERDIFF_OUTPUT_H
#define VERDIFF_OUTPUT_H

#include "common.h"

/*
 * write_summary: 
 * Dumps the pretty ASCII-art summary of what changed directly to standard out.
 * 
 * write_detailed_report: 
 * The forensic autopsy. Writes full block-alignments, size differentials, 
 * and line diffs out to your requested output report file. Spits out an 
 * unholy amount of text if your directories share nothing in common.
 */
int write_summary(FILE *stream, const Config *config, const RunStats *stats, const ResultSet *results, const char *report_path);
int write_detailed_report(FILE *stream, const Config *config, const RunStats *stats, const ResultSet *results);

#endif
