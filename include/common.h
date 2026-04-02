#ifndef VERDIFF_COMMON_H
#define VERDIFF_COMMON_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

typedef enum {
    CHANGE_UNCHANGED = 0,
    CHANGE_MODIFIED = 1,
    CHANGE_ADDED = 2,
    CHANGE_REMOVED = 3
} ChangeType;

typedef enum {
    DETAIL_NONE = 0,
    DETAIL_LINES = 1,
    DETAIL_SIZE_CHANGED = 2,
    DETAIL_BINARY_CHANGED = 3,
    DETAIL_ONLY_IN_TARGET = 4,
    DETAIL_ONLY_IN_SOURCE = 5
} DetailKind;

typedef struct {
    char *data;
    size_t length;
} OwnedString;

typedef struct {
    size_t line_number;
    OwnedString left;
    OwnedString right;
} LineDiff;

typedef struct {
    LineDiff *items;
    size_t count;
    size_t capacity;
} LineDiffVector;

typedef struct PathTreeNode PathTreeNode;

typedef struct {
    const char *path;
    ChangeType type;
    DetailKind detail_kind;
    off_t size_a;
    off_t size_b;
    LineDiffVector line_diffs;
} ChangeRecord;

typedef struct {
    ChangeRecord *items;
    size_t count;
    size_t capacity;
    size_t unchanged_count;
    size_t modified_count;
    size_t added_count;
    size_t removed_count;
    PathTreeNode *ordered_root;
    pthread_mutex_t mutex;
    bool initialized;
} ResultSet;

typedef struct {
    const char *root_a;
    const char *root_b;
    const char *output_path;
    size_t thread_count;
    bool include_lines;
    bool include_unchanged;
    size_t hash_buffer_size;
    size_t mmap_threshold;
    bool verify_equal_hashes;
} Config;

typedef struct ArenaChunk {
    struct ArenaChunk *next;
    size_t capacity;
    size_t used;
    char data[];
} ArenaChunk;

typedef struct {
    ArenaChunk *head;
    size_t chunk_size;
} StringArena;

typedef struct {
    const char *path;
    off_t size;
    time_t mtime;
} FileInfo;

typedef struct {
    const char *key;
    FileInfo value;
    uint64_t hash;
    bool occupied;
} IndexSlot;

typedef struct {
    IndexSlot *slots;
    size_t capacity;
    size_t count;
    StringArena arena;
} FileIndex;

typedef struct {
    const char *relative_path;
} CompareTask;

typedef struct {
    CompareTask *items;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool shutdown;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} TaskQueue;

typedef struct {
    size_t files_in_a;
    size_t files_in_b;
    size_t total_files_scanned;
    time_t scan_time;
} RunStats;

typedef struct CompareContext CompareContext;
typedef struct ProgressState ProgressState;

int string_arena_init(StringArena *arena, size_t chunk_size);
void string_arena_destroy(StringArena *arena);
const char *string_arena_copy(StringArena *arena, const char *value);

int line_diff_vector_push(LineDiffVector *vector, size_t line_number, const char *left, size_t left_len, const char *right, size_t right_len);
void line_diff_vector_destroy(LineDiffVector *vector);

int result_set_init(ResultSet *set);
void result_set_destroy(ResultSet *set);
int result_set_push(ResultSet *set, const ChangeRecord *record);
int result_set_note_type(ResultSet *set, ChangeType type);

uint64_t vd_hash_path(const char *key);
char *vd_join_path(const char *root, const char *relative);
char *vd_normalize_joined_relative(const char *parent_rel, const char *name);
int vd_compare_binary_files(const char *path_a, const char *path_b, size_t buffer_size, bool *equal_out);
bool vd_is_likely_text_file(const char *path);

#endif
