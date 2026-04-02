#include "output.h"

#include "path_tree.h"

#include <string.h>

typedef struct {
    FILE *stream;
    const ResultSet *results;
} OutputTraversal;

static int write_report_header(FILE *stream, const Config *config, const RunStats *stats) {
    char timestamp[32];
    struct tm tm_value;
    localtime_r(&stats->scan_time, &tm_value);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_value);

    if (fputs("==========================================================================================\n", stream) == EOF) {
        return -1;
    }
    if (fputs("                                 VERDIFF COMPARISON REPORT\n", stream) == EOF) {
        return -1;
    }
    if (fputs("==========================================================================================\n\n", stream) == EOF) {
        return -1;
    }
    if (fprintf(stream, "Source Directory : %s\n", config->root_a) < 0) {
        return -1;
    }
    if (fprintf(stream, "Target Directory : %s\n", config->root_b) < 0) {
        return -1;
    }
    if (fprintf(stream, "Scan Time        : %s\n", timestamp) < 0) {
        return -1;
    }
    if (fprintf(stream, "Threads Used     : %zu\n\n", config->thread_count) < 0) {
        return -1;
    }
    return 0;
}

static const char *status_name(ChangeType type) {
    switch (type) {
        case CHANGE_UNCHANGED:
            return "UNCHANGED";
        case CHANGE_MODIFIED:
            return "MODIFIED";
        case CHANGE_ADDED:
            return "ADDED";
        case CHANGE_REMOVED:
            return "REMOVED";
    }
    return "UNKNOWN";
}

static void format_size(off_t size, char *buffer, size_t buffer_len) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = (double)size;
    size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1U < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        unit_index++;
    }
    if (unit_index == 0) {
        snprintf(buffer, buffer_len, "%lld B", (long long)size);
    } else {
        snprintf(buffer, buffer_len, "%.1f %s", value, units[unit_index]);
    }
}

static void clip_text(const char *src, size_t width, char *dest, size_t dest_size) {
    if (width + 1U > dest_size) {
        width = dest_size - 1U;
    }
    size_t len = strlen(src);
    if (len <= width) {
        memcpy(dest, src, len + 1U);
        return;
    }
    if (width <= 3U) {
        memcpy(dest, src, width);
        dest[width] = '\0';
        return;
    }
    memcpy(dest, src, width - 3U);
    memcpy(dest + width - 3U, "...", 4U);
}

static void format_line_summary(const ChangeRecord *record, char *buffer, size_t buffer_len) {
    switch (record->detail_kind) {
        case DETAIL_NONE:
            snprintf(buffer, buffer_len, "-");
            break;
        case DETAIL_LINES: {
            size_t offset = 0;
            offset += (size_t)snprintf(buffer + offset, buffer_len - offset, "Lines: ");
            for (size_t i = 0; i < record->line_diffs.count && offset < buffer_len; ++i) {
                offset += (size_t)snprintf(
                    buffer + offset,
                    buffer_len - offset,
                    "%s%zu",
                    (i == 0) ? "" : ", ",
                    record->line_diffs.items[i].line_number
                );
            }
            break;
        }
        case DETAIL_SIZE_CHANGED:
            snprintf(buffer, buffer_len, "Size Changed (line scan skipped)");
            break;
        case DETAIL_BINARY_CHANGED:
            snprintf(buffer, buffer_len, "Binary Changed");
            break;
        case DETAIL_ONLY_IN_TARGET:
            snprintf(buffer, buffer_len, "New File");
            break;
        case DETAIL_ONLY_IN_SOURCE:
            snprintf(buffer, buffer_len, "Missing in Target");
            break;
    }
}

static int write_record_row(size_t record_index, void *user_data) {
    OutputTraversal *state = user_data;
    const ChangeRecord *record = &state->results->items[record_index];
    if (record->type == CHANGE_UNCHANGED) {
        return 0;
    }
    char size_a[32];
    char size_b[32];
    char detail[512];
    char clipped_path[48];
    if (record->type == CHANGE_ADDED) {
        snprintf(size_a, sizeof(size_a), "-");
    } else {
        format_size(record->size_a, size_a, sizeof(size_a));
    }
    if (record->type == CHANGE_REMOVED) {
        snprintf(size_b, sizeof(size_b), "-");
    } else {
        format_size(record->size_b, size_b, sizeof(size_b));
    }
    format_line_summary(record, detail, sizeof(detail));
    clip_text(record->path, 22, clipped_path, sizeof(clipped_path));

    return fprintf(
        state->stream,
        "| %-22s | %-10s | %-12s | %-12s | %-37s |\n",
        clipped_path,
        status_name(record->type),
        size_a,
        size_b,
        detail
    ) < 0 ? -1 : 0;
}

static int write_detailed_diff(size_t record_index, void *user_data) {
    OutputTraversal *state = user_data;
    const ChangeRecord *record = &state->results->items[record_index];
    if (record->detail_kind != DETAIL_LINES || record->line_diffs.count == 0) {
        return 0;
    }

    FILE *stream = state->stream;
    if (fprintf(stream, "\nFile: %s\nStatus: %s\n\n", record->path, status_name(record->type)) < 0) {
        return -1;
    }
    if (fputs("---------------------------------------------------------------------------------------------------------------\n", stream) == EOF) {
        return -1;
    }
    if (fputs("| LINE | FILE A                               | FILE B                               |\n", stream) == EOF) {
        return -1;
    }
    if (fputs("---------------------------------------------------------------------------------------------------------------\n", stream) == EOF) {
        return -1;
    }

    for (size_t i = 0; i < record->line_diffs.count; ++i) {
        const LineDiff *diff = &record->line_diffs.items[i];
        char left[40];
        char right[40];
        clip_text(diff->left.data, 36, left, sizeof(left));
        clip_text(diff->right.data, 36, right, sizeof(right));
        if (fprintf(stream, "| %-4zu | %-36s | %-36s |\n", diff->line_number, left, right) < 0) {
            return -1;
        }
    }

    if (fputs("---------------------------------------------------------------------------------------------------------------\n", stream) == EOF) {
        return -1;
    }
    return 0;
}

int write_summary(FILE *stream, const Config *config, const RunStats *stats, const ResultSet *results, const char *report_path) {
    if (write_report_header(stream, config, stats) != 0) {
        return -1;
    }
    if (fputs("------------------------------------------------------------------------------------------\nSUMMARY\n------------------------------------------------------------------------------------------\n", stream) == EOF) {
        return -1;
    }
    if (fprintf(stream, "Total Files Scanned   : %zu\n", stats->total_files_scanned) < 0) {
        return -1;
    }
    if (fprintf(stream, "Unchanged Files       : %zu\n", results->unchanged_count) < 0) {
        return -1;
    }
    if (fprintf(stream, "Modified Files        : %zu\n", results->modified_count) < 0) {
        return -1;
    }
    if (fprintf(stream, "Added Files           : %zu\n", results->added_count) < 0) {
        return -1;
    }
    if (fprintf(stream, "Removed Files         : %zu\n\n", results->removed_count) < 0) {
        return -1;
    }
    if (fprintf(stream, "Detailed Report File  : %s\n", report_path) < 0) {
        return -1;
    }
    if (fputs("\n==========================================================================================\nEND OF SUMMARY\n==========================================================================================\n", stream) == EOF) {
        return -1;
    }
    return 0;
}

int write_detailed_report(FILE *stream, const Config *config, const RunStats *stats, const ResultSet *results) {
    if (write_report_header(stream, config, stats) != 0) {
        return -1;
    }
    if (fputs("------------------------------------------------------------------------------------------\nSUMMARY\n------------------------------------------------------------------------------------------\n", stream) == EOF) {
        return -1;
    }
    if (fprintf(stream, "Total Files Scanned   : %zu\n", stats->total_files_scanned) < 0) {
        return -1;
    }
    if (fprintf(stream, "Unchanged Files       : %zu\n", results->unchanged_count) < 0) {
        return -1;
    }
    if (fprintf(stream, "Modified Files        : %zu\n", results->modified_count) < 0) {
        return -1;
    }
    if (fprintf(stream, "Added Files           : %zu\n", results->added_count) < 0) {
        return -1;
    }
    if (fprintf(stream, "Removed Files         : %zu\n\n", results->removed_count) < 0) {
        return -1;
    }
    if (fputs("==========================================================================================\nDETAILED COMPARISON\n==========================================================================================\n\n", stream) == EOF) {
        return -1;
    }
    if (fputs("---------------------------------------------------------------------------------------------------------------\n", stream) == EOF) {
        return -1;
    }
    if (fputs("| FILE PATH              | STATUS     | SIZE (FILE A) | SIZE (FILE B) | LINE DIFFERENCES                      |\n", stream) == EOF) {
        return -1;
    }
    if (fputs("---------------------------------------------------------------------------------------------------------------\n\n", stream) == EOF) {
        return -1;
    }

    OutputTraversal traversal = {
        .stream = stream,
        .results = results,
    };
    if (path_tree_inorder(results->ordered_root, write_record_row, &traversal) != 0) {
        return -1;
    }
    if (fputs("\n---------------------------------------------------------------------------------------------------------------\n\n", stream) == EOF) {
        return -1;
    }

    if (fputs("==========================================================================================\nSIDE-BY-SIDE LINE DIFFERENCE (DETAILED VIEW)\n==========================================================================================\n", stream) == EOF) {
        return -1;
    }
    if (path_tree_inorder(results->ordered_root, write_detailed_diff, &traversal) != 0) {
        return -1;
    }
    if (fputs("\n==========================================================================================\nEND OF REPORT\n==========================================================================================\n", stream) == EOF) {
        return -1;
    }
    return 0;
}
