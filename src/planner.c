#include "compare.h"

#include "index.h"

#include <errno.h>
#include <string.h>

static int enqueue_candidate(TaskQueue *queue, const char *path, ProgressState *progress) {
    CompareTask task = {
        .relative_path = path,
    };
    progress_note_compare_start(progress, path);
    return task_queue_push(queue, task);
}

static int push_presence_record(ResultSet *results, const char *path, ChangeType type, DetailKind detail_kind, off_t size_a, off_t size_b) {
    ChangeRecord record;
    memset(&record, 0, sizeof(record));
    record.path = path;
    record.type = type;
    record.detail_kind = detail_kind;
    record.size_a = size_a;
    record.size_b = size_b;
    return result_set_push(results, &record);
}

int plan_and_compare(const Config *config, const FileIndex *index_a, const FileIndex *index_b, ResultSet *results, ProgressState *progress) {
    CompareContext context = {
        .config = config,
        .index_a = index_a,
        .index_b = index_b,
        .progress = progress,
        .results = results,
        .first_error = 0,
    };
    int rc = pthread_mutex_init(&context.error_mutex, NULL);
    if (rc != 0) {
        return rc;
    }

    rc = task_queue_init(&context.queue, 4096);
    if (rc != 0) {
        pthread_mutex_destroy(&context.error_mutex);
        return rc;
    }

    ThreadPool pool = {0};
    rc = thread_pool_init(&pool, &context, config->thread_count);
    if (rc != 0) {
        task_queue_destroy(&context.queue);
        pthread_mutex_destroy(&context.error_mutex);
        return rc;
    }

    for (size_t i = 0; i < index_a->capacity && rc == 0; ++i) {
        if (!index_a->slots[i].occupied) {
            continue;
        }
        const char *path = index_a->slots[i].key;
        const FileInfo *info_a = &index_a->slots[i].value;
        const FileInfo *other = file_index_find_const(index_b, path);
        if (other == NULL) {
            rc = push_presence_record(results, path, CHANGE_REMOVED, DETAIL_ONLY_IN_SOURCE, info_a->size, 0);
            continue;
        }
        if (info_a->size != other->size) {
            rc = push_presence_record(results, path, CHANGE_MODIFIED, DETAIL_SIZE_CHANGED, info_a->size, other->size);
            continue;
        }
        rc = enqueue_candidate(&context.queue, path, progress);
    }

    for (size_t i = 0; i < index_b->capacity && rc == 0; ++i) {
        if (!index_b->slots[i].occupied) {
            continue;
        }
        const char *path = index_b->slots[i].key;
        if (file_index_find_const(index_a, path) == NULL) {
            rc = push_presence_record(results, path, CHANGE_ADDED, DETAIL_ONLY_IN_TARGET, 0, index_b->slots[i].value.size);
        }
    }

    task_queue_shutdown(&context.queue);
    thread_pool_join(&pool);

    if (rc == 0) {
        rc = compare_context_get_error(&context);
    }

    task_queue_destroy(&context.queue);
    pthread_mutex_destroy(&context.error_mutex);
    return rc;
}
