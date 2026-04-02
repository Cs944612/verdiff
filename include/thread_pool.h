#ifndef VERDIFF_THREAD_POOL_H
#define VERDIFF_THREAD_POOL_H

#include "common.h"
#include "progress.h"

/*
 * CompareContext is basically the global playbook for our threads.
 * Contains read-only references to configurations, the thread-safe work queue, 
 * the thread-safe result set, and a mutex to securely record the FIRST catastrophic 
 * error we encountered (because realistically, we're only interested in the first one).
 */
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

/* 
 * ThreadPool:
 * A bounded squad of reliable POSIX threads. We spawn 'thread_count' of them, 
 * point them at `worker_main()`, and lazily watch the CPUs catch fire as they gorge 
 * themselves on identical CompareTasks.
 */
typedef struct {
    pthread_t *threads;
    size_t thread_count;
    CompareContext *context;
} ThreadPool;

/*
 * Queue primitives. 
 * Warning: Under heavy load, push/pop block aggressively on conditional variables.
 * task_queue_shutdown() broadcasts an absolute "everybody go home right now" 
 * signal capable of shaking blocked threads out of their slumber.
 */
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
