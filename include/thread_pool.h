#ifndef VERDIFF_THREAD_POOL_H
#define VERDIFF_THREAD_POOL_H

#include "common.h"
#include "progress.h"

struct CompareContext {
    const Config *config;
    const FileIndex *index_a;
    const FileIndex *index_b;
    ProgressState *progress;
    TaskQueue queue;
    ResultSet *results;
    pthread_mutex_t error_mutex;
    int first_error;
};

typedef struct {
    pthread_t *threads;
    size_t thread_count;
    CompareContext *context;
} ThreadPool;

int task_queue_init(TaskQueue *queue, size_t capacity);
void task_queue_destroy(TaskQueue *queue);
int task_queue_push(TaskQueue *queue, CompareTask task);
bool task_queue_pop(TaskQueue *queue, CompareTask *task_out);
void task_queue_shutdown(TaskQueue *queue);

int thread_pool_init(ThreadPool *pool, CompareContext *context, size_t thread_count);
void thread_pool_join(ThreadPool *pool);
void compare_context_set_error(CompareContext *context, int error_code);
int compare_context_get_error(CompareContext *context);

#endif
