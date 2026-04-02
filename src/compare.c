#include "compare.h"

#include "hash.h"
#include "index.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t trim_line_length(const char *line, size_t len) {
    while (len > 0 && (line[len - 1U] == '\n' || line[len - 1U] == '\r')) {
        len--;
    }
    return len;
}

int detect_line_differences(const char *path_a, const char *path_b, LineDiffVector *lines) {
    FILE *file_a = fopen(path_a, "rb");
    if (file_a == NULL) {
        return errno;
    }
    FILE *file_b = fopen(path_b, "rb");
    if (file_b == NULL) {
        int saved = errno;
        fclose(file_a);
        return saved;
    }

    char *line_a = NULL;
    char *line_b = NULL;
    size_t cap_a = 0;
    size_t cap_b = 0;
    ssize_t len_a;
    ssize_t len_b;
    size_t line_no = 1;
    int rc = 0;

    for (;;) {
        errno = 0;
        len_a = getline(&line_a, &cap_a, file_a);
        if (len_a < 0 && ferror(file_a)) {
            rc = errno != 0 ? errno : EIO;
            break;
        }
        errno = 0;
        len_b = getline(&line_b, &cap_b, file_b);
        if (len_b < 0 && ferror(file_b)) {
            rc = errno != 0 ? errno : EIO;
            break;
        }
        if (len_a < 0 && len_b < 0) {
            break;
        }

        bool differs = false;
        size_t left_len = len_a < 0 ? 0U : trim_line_length(line_a, (size_t)len_a);
        size_t right_len = len_b < 0 ? 0U : trim_line_length(line_b, (size_t)len_b);
        if (len_a != len_b) {
            differs = true;
        } else if (len_a >= 0 && memcmp(line_a, line_b, (size_t)len_a) != 0) {
            differs = true;
        }

        if (differs) {
            rc = line_diff_vector_push(
                lines,
                line_no,
                len_a < 0 ? "" : line_a,
                left_len,
                len_b < 0 ? "" : line_b,
                right_len
            );
            if (rc != 0) {
                break;
            }
        }
        line_no++;
    }

    free(line_a);
    free(line_b);
    fclose(file_a);
    fclose(file_b);
    return rc;
}

static int push_simple_record(ResultSet *results, const char *path, ChangeType type, DetailKind detail_kind, off_t size_a, off_t size_b) {
    ChangeRecord record;
    memset(&record, 0, sizeof(record));
    record.path = path;
    record.type = type;
    record.detail_kind = detail_kind;
    record.size_a = size_a;
    record.size_b = size_b;
    return result_set_push(results, &record);
}

int compare_candidate_file(const CompareContext *context, const char *relative_path) {
    const FileInfo *info_a = file_index_find_const(context->index_a, relative_path);
    const FileInfo *info_b = file_index_find_const(context->index_b, relative_path);
    if (info_a == NULL || info_b == NULL) {
        return ENOENT;
    }

    if (info_a->size != info_b->size) {
        return push_simple_record(context->results, relative_path, CHANGE_MODIFIED, DETAIL_SIZE_CHANGED, info_a->size, info_b->size);
    }

    char *path_a = vd_join_path(context->config->root_a, relative_path);
    char *path_b = vd_join_path(context->config->root_b, relative_path);
    if (path_a == NULL || path_b == NULL) {
        free(path_a);
        free(path_b);
        return ENOMEM;
    }

    bool equal = false;
    int rc = vd_compare_binary_files(path_a, path_b, context->config->hash_buffer_size, &equal);
    if (rc == 0 && equal) {
        if (context->config->include_unchanged) {
            rc = push_simple_record(context->results, relative_path, CHANGE_UNCHANGED, DETAIL_NONE, info_a->size, info_b->size);
        } else {
            rc = result_set_note_type(context->results, CHANGE_UNCHANGED);
        }
        free(path_a);
        free(path_b);
        return rc;
    }

    if (rc == 0) {
        ChangeRecord record;
        memset(&record, 0, sizeof(record));
        record.path = relative_path;
        record.type = CHANGE_MODIFIED;
        record.detail_kind = DETAIL_BINARY_CHANGED;
        record.size_a = info_a->size;
        record.size_b = info_b->size;

        if (context->config->include_lines &&
            vd_is_likely_text_file(path_a) &&
            vd_is_likely_text_file(path_b)) {
            rc = detect_line_differences(path_a, path_b, &record.line_diffs);
            if (rc == 0 && record.line_diffs.count > 0) {
                record.detail_kind = DETAIL_LINES;
            }
        }

        if (rc == 0) {
            rc = result_set_push(context->results, &record);
        }
        if (rc != 0) {
            line_diff_vector_destroy(&record.line_diffs);
        }
    }

    free(path_a);
    free(path_b);
    return rc;
}
