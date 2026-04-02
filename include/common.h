#ifndef VERDIFF_COMMON_H
#define VERDIFF_COMMON_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

/* 
 * ChangeType: The core verdict of our comparison. 
 * Are things exactly as they were, or did someone creatively destroy something?
 * 
 * 0 = UNCHANGED (Ah, peace)
 * 1 = MODIFIED  (Someone touched the spaghetti)
 * 2 = ADDED     (Look! New spaghetti!)
 * 3 = REMOVED   (Where did my spaghetti go!?)
 */
typedef enum {
    CHANGE_UNCHANGED = 0,
    CHANGE_MODIFIED = 1,
    CHANGE_ADDED = 2,
    CHANGE_REMOVED = 3
} ChangeType;

/* 
 * DetailKind: Because saying "it changed" isn't helpful enough to debug a 2AM outage.
 * We need to know *how* it changed. Did they append a newline, or rewrite the binary?
 */
typedef enum {
    DETAIL_NONE = 0,
    DETAIL_LINES = 1,           /* Hey, we actually found explicit line differences! */
    DETAIL_SIZE_CHANGED = 2,    /* Fast rejection: sizes differ, we didn't even read the file. */
    DETAIL_BINARY_CHANGED = 3,  /* Same size, different bytes. Spooky! */
    DETAIL_ONLY_IN_TARGET = 4,
    DETAIL_ONLY_IN_SOURCE = 5
} DetailKind;

/*
 * A painfully simple owned string, used so we don't accidentally free something
 * we shouldn't, or leak something we should. Don't forget to clean this up!
 */
typedef struct {
    char *data;
    size_t length;
} OwnedString;

/* 
 * LineDiff tracks exactly what went wonderfully wrong on a specific line.
 * 'left' is what you used to have, 'right' is what you have now.
 * We store the strings here because we hate scrolling back in code.
 */
typedef struct {
    size_t line_number;
    OwnedString left;
    OwnedString right;
} LineDiff;

/* A growable dynamic array for LineDiffs. Because one bug is never enough. */
typedef struct {
    LineDiff *items;
    size_t count;
    size_t capacity;
} LineDiffVector;

typedef struct PathTreeNode PathTreeNode;

/*
 * ChangeRecord: The holy grail for a single file. 
 * Once a worker thread figures out what happened, it packs the truth in here
 * and ships it to the ResultSet. 
 */
typedef struct {
    const char *path;
    ChangeType type;
    DetailKind detail_kind;
    off_t size_a;
    off_t size_b;
    LineDiffVector line_diffs;
} ChangeRecord;

/*
 * ResultSet: The thread-safe vault where we store our verdicts.
 * 
 * We keep track of stats here inline so we don't need to iterate later.
 * It's protected by a mutex, because parallel threads are chaotic creatures
 * that will trample each other to write their results.
 */
typedef struct {
    ChangeRecord *items;
    size_t count;
    size_t capacity;
    size_t unchanged_count;
    size_t modified_count;
    size_t added_count;
    size_t removed_count;
    PathTreeNode *ordered_root; /* Custom AVL tree out of pure spite for sorting later */
    pthread_mutex_t mutex;
    bool initialized;
} ResultSet;

/*
 * Config: Holds everything the user asked us to do.
 * If things go wrong, it's mostly because of parameter tuning here. 
 * 'verify_equal_hashes' protects us against astronomically rare hash collisions.
 */
typedef struct {
    const char *root_a;
    const char *root_b;
    const char *output_path;
    size_t thread_count;
    bool include_lines;
    bool include_unchanged;
    size_t hash_buffer_size;
    size_t mmap_threshold;      /* Files below this size get the fast zero-copy mmap path */
    bool verify_equal_hashes;   /* Because we don't trust our own math */
} Config;

/*
 * Simple memory arena chunk. Instead of mallocing 10,000 tiny strings 
 * representing file paths, we allocate massive chunks and just increment a pointer.
 * This is arguably the biggest contributor to our speed outside of threading.
 */
typedef struct ArenaChunk {
    struct ArenaChunk *next;
    size_t capacity;
    size_t used;
    char data[];
} ArenaChunk;

/* The arena manager. Feed it memory, and it will keep your paths safe. */
typedef struct {
    ArenaChunk *head;
    size_t chunk_size;
} StringArena;

/* Basic metadata for a file. Small, efficient, fits nicely in a cache line. */
typedef struct {
    const char *path;
    off_t size;
    time_t mtime;
} FileInfo;

/* A slot in our custom Robin-Hood-esque linear probing hash table. */
typedef struct {
    const char *key;
    FileInfo value;
    uint64_t hash;
    bool occupied;
} IndexSlot;

/* 
 * FileIndex: A high-performance hash map for paths.
 * Since comparing paths by strings is brutally slow, we hash the paths 
 * and do linear probing. This runs circles around standard POSIX search patterns.
 */
typedef struct {
    IndexSlot *slots;
    size_t capacity;
    size_t count;
    StringArena arena; /* Arena where the actual string paths live forever */
} FileIndex;

/* Just a simple job ticket for a worker thread: "Compare this file" */
typedef struct {
    const char *relative_path;
} CompareTask;

/*
 * TaskQueue: A classic producer-consumer queue backed by a ring buffer.
 * Protected by a mutex and two condition variables (not_empty, not_full).
 * Do NOT mess with the sleep/wake patterns here unless you enjoy deadlocks.
 */
typedef struct {
    CompareTask *items;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool shutdown;              /* Set to true when the party is over */
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;   /* Consumers listen here */
    pthread_cond_t not_full;    /* Producer listens here */
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
