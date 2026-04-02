#include "arena.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int string_arena_init(StringArena *arena, size_t chunk_size) {
    arena->head = NULL;
    arena->chunk_size = chunk_size;
    return 0;
}

void string_arena_destroy(StringArena *arena) {
    ArenaChunk *chunk = arena->head;
    while (chunk != NULL) {
        ArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    arena->head = NULL;
}

static ArenaChunk *arena_alloc_chunk(StringArena *arena, size_t min_size) {
    size_t capacity = arena->chunk_size;
    if (capacity < min_size) {
        capacity = min_size;
    }
    ArenaChunk *chunk = malloc(sizeof(*chunk) + capacity);
    if (chunk == NULL) {
        return NULL;
    }
    chunk->next = arena->head;
    chunk->capacity = capacity;
    chunk->used = 0;
    arena->head = chunk;
    return chunk;
}

/*
 * string_arena_copy:
 * The absolute fastest way to "allocate" a string. We check if it fits in the 
 * current continuous heap chunk. If yes, blindly bump the pointer forward. 
 * If no, fetch a massive new chunk from the OS, tack it onto our linked list, 
 * and then bump the pointer. No free() calls required until the apocalypse 
 * (program exit).
 */
const char *string_arena_copy(StringArena *arena, const char *value) {
    size_t len = strlen(value) + 1U;
    ArenaChunk *chunk = arena->head;
    if (chunk == NULL || chunk->capacity - chunk->used < len) {
        chunk = arena_alloc_chunk(arena, len);
        if (chunk == NULL) {
            return NULL;
        }
    }

    char *dest = chunk->data + chunk->used;
    memcpy(dest, value, len);
    chunk->used += len;
    return dest;
}
