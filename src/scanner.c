#include "scanner.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char *relative;
    char *absolute;
} ScanFrame;

typedef struct {
    ScanFrame *items;
    size_t count;
    size_t capacity;
} FrameStack;

static void frame_stack_destroy(FrameStack *stack) {
    for (size_t i = 0; i < stack->count; ++i) {
        free(stack->items[i].relative);
        free(stack->items[i].absolute);
    }
    free(stack->items);
}

static int frame_stack_push(FrameStack *stack, ScanFrame frame) {
    if (stack->count == stack->capacity) {
        size_t new_capacity = stack->capacity == 0 ? 32 : stack->capacity << 1U;
        ScanFrame *new_items = realloc(stack->items, new_capacity * sizeof(*new_items));
        if (new_items == NULL) {
            return ENOMEM;
        }
        stack->items = new_items;
        stack->capacity = new_capacity;
    }
    stack->items[stack->count++] = frame;
    return 0;
}

static bool frame_stack_pop(FrameStack *stack, ScanFrame *frame_out) {
    if (stack->count == 0) {
        return false;
    }
    *frame_out = stack->items[--stack->count];
    return true;
}

/*
 * scan_directory
 * Uses an explicit heap queue (FrameStack) to traverse the directory natively.
 * We do this so the program doesn't explode via stack overflow when somebody 
 * nests 1,000 folders deep because they thought Java package structures should 
 * exactly mirror their corporate org chart.
 */
int scan_directory(const char *root, FileIndex *index, ProgressState *progress, ProgressPhase phase) {
    FrameStack stack = {0};
    int rc = 0;
    ScanFrame frame = {
        .relative = strdup(""),
        .absolute = strdup(root),
    };
    if (frame.relative == NULL || frame.absolute == NULL) {
        free(frame.relative);
        free(frame.absolute);
        return ENOMEM;
    }

    rc = frame_stack_push(&stack, frame);
    if (rc != 0) {
        frame_stack_destroy(&stack);
        return rc;
    }

    while (frame_stack_pop(&stack, &frame)) {
        DIR *dir = opendir(frame.absolute);
        if (dir == NULL) {
            rc = errno;
            free(frame.relative);
            free(frame.absolute);
            break;
        }

        struct dirent *entry = NULL;
        errno = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char *rel = vd_normalize_joined_relative(frame.relative, entry->d_name);
            char *abs = vd_join_path(frame.absolute, entry->d_name);
            if (rel == NULL || abs == NULL) {
                free(rel);
                free(abs);
                rc = ENOMEM;
                break;
            }

            struct stat st;
            if (lstat(abs, &st) != 0) {
                rc = errno;
                free(rel);
                free(abs);
                break;
            }

            if (S_ISDIR(st.st_mode)) {
                ScanFrame next = {
                    .relative = rel,
                    .absolute = abs,
                };
                rc = frame_stack_push(&stack, next);
                if (rc != 0) {
                    free(rel);
                    free(abs);
                    break;
                }
            } else if (S_ISREG(st.st_mode)) {
                rc = file_index_upsert(index, rel, st.st_size, st.st_mtime);
                if (rc == 0) {
                    progress_note_scan(progress, phase, rel);
                }
                free(rel);
                free(abs);
                if (rc != 0) {
                    break;
                }
            } else {
                free(rel);
                free(abs);
            }
            errno = 0;
        }

        if (entry == NULL && errno != 0 && rc == 0) {
            rc = errno;
        }

        closedir(dir);
        free(frame.relative);
        free(frame.absolute);
        if (rc != 0) {
            break;
        }
    }

    if (rc != 0) {
        frame_stack_destroy(&stack);
    }
    return rc;
}
