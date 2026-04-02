#include "index.h"

#include "path_tree.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static size_t next_pow2(size_t value) {
    size_t n = 16;
    while (n < value) {
        n <<= 1U;
    }
    return n;
}

uint64_t vd_hash_path(const char *key) {
    uint64_t hash = 1469598103934665603ULL;
    const unsigned char *ptr = (const unsigned char *)key;
    while (*ptr != '\0') {
        hash ^= (uint64_t)(*ptr++);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static size_t probe_index(uint64_t hash, size_t capacity) {
    return (size_t)(hash & (uint64_t)(capacity - 1U));
}

/*
 * file_index_rehash:
 * When the neighborhood gets too crowded (>70% load factor), we build a brand 
 * new universe physically twice as large and migrate everybody over. 
 * This prevents our beloved linear probing from degrading into O(N) sadness.
 */
static int file_index_rehash(FileIndex *index, size_t new_capacity) {
    IndexSlot *old_slots = index->slots;
    size_t old_capacity = index->capacity;
    IndexSlot *new_slots = calloc(new_capacity, sizeof(*new_slots));
    if (new_slots == NULL) {
        return ENOMEM;
    }

    index->slots = new_slots;
    index->capacity = new_capacity;
    index->count = 0;

    for (size_t i = 0; i < old_capacity; ++i) {
        if (!old_slots[i].occupied) {
            continue;
        }

        size_t pos = probe_index(old_slots[i].hash, new_capacity);
        while (index->slots[pos].occupied) {
            pos = (pos + 1U) & (new_capacity - 1U);
        }
        index->slots[pos] = old_slots[i];
        index->count++;
    }

    free(old_slots);
    return 0;
}

int file_index_init(FileIndex *index, size_t initial_capacity) {
    size_t capacity = next_pow2(initial_capacity == 0 ? 16 : initial_capacity);
    index->slots = calloc(capacity, sizeof(*index->slots));
    if (index->slots == NULL) {
        return ENOMEM;
    }
    index->capacity = capacity;
    index->count = 0;
    return string_arena_init(&index->arena, 1U << 20U);
}

void file_index_destroy(FileIndex *index) {
    if (index == NULL) {
        return;
    }
    free(index->slots);
    index->slots = NULL;
    index->capacity = 0;
    index->count = 0;
    string_arena_destroy(&index->arena);
}

static FileInfo *file_index_find_slot(FileIndex *index, const char *path, size_t *slot_out, uint64_t hash) {
    if (index->capacity == 0) {
        return NULL;
    }

    size_t pos = probe_index(hash, index->capacity);
    while (index->slots[pos].occupied) {
        if (index->slots[pos].hash == hash && strcmp(index->slots[pos].key, path) == 0) {
            if (slot_out != NULL) {
                *slot_out = pos;
            }
            return &index->slots[pos].value;
        }
        pos = (pos + 1U) & (index->capacity - 1U);
    }

    if (slot_out != NULL) {
        *slot_out = pos;
    }
    return NULL;
}

FileInfo *file_index_find(FileIndex *index, const char *path) {
    return file_index_find_slot(index, path, NULL, vd_hash_path(path));
}

const FileInfo *file_index_find_const(const FileIndex *index, const char *path) {
    return file_index_find_slot((FileIndex *)index, path, NULL, vd_hash_path(path));
}

int file_index_upsert(FileIndex *index, const char *path, off_t size, time_t mtime) {
    if ((index->count + 1U) * 10U >= index->capacity * 7U) {
        int rc = file_index_rehash(index, index->capacity << 1U);
        if (rc != 0) {
            return rc;
        }
    }

    uint64_t hash = vd_hash_path(path);
    size_t slot = 0;
    FileInfo *existing = file_index_find_slot(index, path, &slot, hash);
    if (existing != NULL) {
        existing->size = size;
        existing->mtime = mtime;
        return 0;
    }

    const char *stored = string_arena_copy(&index->arena, path);
    if (stored == NULL) {
        return ENOMEM;
    }

    index->slots[slot].key = stored;
    index->slots[slot].hash = hash;
    index->slots[slot].occupied = true;
    index->slots[slot].value.path = stored;
    index->slots[slot].value.size = size;
    index->slots[slot].value.mtime = mtime;
    index->count++;
    return 0;
}

static int owned_string_copy(OwnedString *dest, const char *src, size_t len) {
    dest->data = malloc(len + 1U);
    if (dest->data == NULL) {
        return ENOMEM;
    }
    memcpy(dest->data, src, len);
    dest->data[len] = '\0';
    dest->length = len;
    return 0;
}

int line_diff_vector_push(LineDiffVector *vector, size_t line_number, const char *left, size_t left_len, const char *right, size_t right_len) {
    if (vector->count == vector->capacity) {
        size_t new_capacity = vector->capacity == 0 ? 8 : vector->capacity << 1U;
        LineDiff *new_items = realloc(vector->items, new_capacity * sizeof(*new_items));
        if (new_items == NULL) {
            return ENOMEM;
        }
        vector->items = new_items;
        vector->capacity = new_capacity;
    }

    LineDiff *diff = &vector->items[vector->count];
    diff->line_number = line_number;
    diff->left.data = NULL;
    diff->left.length = 0;
    diff->right.data = NULL;
    diff->right.length = 0;

    int rc = owned_string_copy(&diff->left, left, left_len);
    if (rc != 0) {
        return rc;
    }
    rc = owned_string_copy(&diff->right, right, right_len);
    if (rc != 0) {
        free(diff->left.data);
        diff->left.data = NULL;
        diff->left.length = 0;
        return rc;
    }

    vector->count++;
    return 0;
}

void line_diff_vector_destroy(LineDiffVector *vector) {
    if (vector == NULL) {
        return;
    }
    for (size_t i = 0; i < vector->count; ++i) {
        free(vector->items[i].left.data);
        free(vector->items[i].right.data);
    }
    free(vector->items);
    vector->items = NULL;
    vector->count = 0;
    vector->capacity = 0;
}

int result_set_init(ResultSet *set) {
    memset(set, 0, sizeof(*set));
    int rc = pthread_mutex_init(&set->mutex, NULL);
    if (rc == 0) {
        set->initialized = true;
    }
    return rc;
}

void result_set_destroy(ResultSet *set) {
    if (set == NULL) {
        return;
    }
    for (size_t i = 0; i < set->count; ++i) {
        line_diff_vector_destroy(&set->items[i].line_diffs);
    }
    free(set->items);
    path_tree_destroy(set->ordered_root);
    if (set->initialized) {
        pthread_mutex_destroy(&set->mutex);
        set->initialized = false;
    }
}

static void increment_counter(ResultSet *set, ChangeType type) {
    switch (type) {
        case CHANGE_UNCHANGED:
            set->unchanged_count++;
            break;
        case CHANGE_MODIFIED:
            set->modified_count++;
            break;
        case CHANGE_ADDED:
            set->added_count++;
            break;
        case CHANGE_REMOVED:
            set->removed_count++;
            break;
    }
}

int result_set_push(ResultSet *set, const ChangeRecord *record) {
    int rc = pthread_mutex_lock(&set->mutex);
    if (rc != 0) {
        return rc;
    }

    if (set->count == set->capacity) {
        size_t new_capacity = set->capacity == 0 ? 64 : set->capacity << 1U;
        ChangeRecord *new_items = realloc(set->items, new_capacity * sizeof(*new_items));
        if (new_items == NULL) {
            pthread_mutex_unlock(&set->mutex);
            return ENOMEM;
        }
        set->items = new_items;
        set->capacity = new_capacity;
    }

    size_t index = set->count;
    set->items[index] = *record;
    set->count++;
    increment_counter(set, record->type);

    rc = path_tree_insert(&set->ordered_root, set->items, index);
    if (rc != 0) {
        set->count--;
        switch (record->type) {
            case CHANGE_UNCHANGED:
                set->unchanged_count--;
                break;
            case CHANGE_MODIFIED:
                set->modified_count--;
                break;
            case CHANGE_ADDED:
                set->added_count--;
                break;
            case CHANGE_REMOVED:
                set->removed_count--;
                break;
        }
        pthread_mutex_unlock(&set->mutex);
        return rc;
    }

    pthread_mutex_unlock(&set->mutex);
    return 0;
}

int result_set_note_type(ResultSet *set, ChangeType type) {
    int rc = pthread_mutex_lock(&set->mutex);
    if (rc != 0) {
        return rc;
    }
    increment_counter(set, type);
    pthread_mutex_unlock(&set->mutex);
    return 0;
}

char *vd_join_path(const char *root, const char *relative) {
    size_t root_len = strlen(root);
    size_t rel_len = strlen(relative);
    bool need_sep = root_len > 0 && root[root_len - 1U] != '/';
    size_t total = root_len + (need_sep ? 1U : 0U) + rel_len + 1U;
    char *joined = malloc(total);
    if (joined == NULL) {
        return NULL;
    }

    memcpy(joined, root, root_len);
    size_t offset = root_len;
    if (need_sep) {
        joined[offset++] = '/';
    }
    memcpy(joined + offset, relative, rel_len + 1U);
    return joined;
}

char *vd_normalize_joined_relative(const char *parent_rel, const char *name) {
    size_t parent_len = strlen(parent_rel);
    size_t name_len = strlen(name);
    size_t total = parent_len + (parent_len > 0 ? 1U : 0U) + name_len + 1U;
    char *path = malloc(total);
    if (path == NULL) {
        return NULL;
    }
    size_t offset = 0;
    if (parent_len > 0) {
        memcpy(path, parent_rel, parent_len);
        offset = parent_len;
        path[offset++] = '/';
    }
    memcpy(path + offset, name, name_len + 1U);
    return path;
}
