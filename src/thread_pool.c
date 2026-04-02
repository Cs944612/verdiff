#include "thread_pool.h"

#include "compare.h"

#include <errno.h>
#include <stdlib.h>

int task_queue_init(TaskQueue *queue, size_t capacity) {
    queue->items = calloc(capacity, sizeof(*queue->items));
    if (queue->items == NULL) {
        return ENOMEM;
    }
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->shutdown = false;
    int rc = pthread_mutex_init(&queue->mutex, NULL);
    if (rc != 0) {
        free(queue->items);
        return rc;
    }
    rc = pthread_cond_init(&queue->not_empty, NULL);
    if (rc != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return rc;
    }
    rc = pthread_cond_init(&queue->not_full, NULL);
    if (rc != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return rc;
    }
    return 0;
}

void task_queue_destroy(TaskQueue *queue) {
    if (queue == NULL) {
        return;
    }
    free(queue->items);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->mutex);
}

int task_queue_push(TaskQueue *queue, CompareTask task) {
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        return rc;
    }

    while (!queue->shutdown && queue->count == queue->capacity) {
        rc = pthread_cond_wait(&queue->not_full, &queue->mutex);
        if (rc != 0) {
            pthread_mutex_unlock(&queue->mutex);
            return rc;
        }
    }

    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return ECANCELED;
    }

    queue->items[queue->tail] = task;
    queue->tail = (queue->tail + 1U) % queue->capacity;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

bool task_queue_pop(TaskQueue *queue, CompareTask *task_out) {
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        return false;
    }

    while (!queue->shutdown && queue->count == 0) {
        rc = pthread_cond_wait(&queue->not_empty, &queue->mutex);
        if (rc != 0) {
            pthread_mutex_unlock(&queue->mutex);
            return false;
        }
    }

    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    *task_out = queue->items[queue->head];
    queue->head = (queue->head + 1U) % queue->capacity;
    queue->count--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

void task_queue_shutdown(TaskQueue *queue) {
    if (pthread_mutex_lock(&queue->mutex) != 0) {
        return;
    }
    queue->shutdown = true;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
}

void compare_context_set_error(CompareContext *context, int error_code) {
    if (pthread_mutex_lock(&context->error_mutex) != 0) {
        return;
    }
    if (context->first_error == 0) {
        context->first_error = error_code;
    }
    pthread_mutex_unlock(&context->error_mutex);
}

int compare_context_get_error(CompareContext *context) {
    int error_code = 0;
    if (pthread_mutex_lock(&context->error_mutex) != 0) {
        return EINVAL;
    }
    error_code = context->first_error;
    pthread_mutex_unlock(&context->error_mutex);
    return error_code;
}

static void *worker_main(void *arg) {
    CompareContext *context = arg;
    CompareTask task;
    while (task_queue_pop(&context->queue, &task)) {
        int rc = compare_candidate_file(context, task.relative_path);
        if (rc != 0) {
            compare_context_set_error(context, rc);
            task_queue_shutdown(&context->queue);
            break;
        }
    }
    return NULL;
}

int thread_pool_init(ThreadPool *pool, CompareContext *context, size_t thread_count) {
    pool->threads = calloc(thread_count, sizeof(*pool->threads));
    if (pool->threads == NULL) {
        return ENOMEM;
    }
    pool->thread_count = thread_count;
    pool->context = context;

    for (size_t i = 0; i < thread_count; ++i) {
        int rc = pthread_create(&pool->threads[i], NULL, worker_main, context);
        if (rc != 0) {
            task_queue_shutdown(&context->queue);
            for (size_t j = 0; j < i; ++j) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            pool->threads = NULL;
            return rc;
        }
    }
    return 0;
}

void thread_pool_join(ThreadPool *pool) {
    if (pool == NULL || pool->threads == NULL) {
        return;
    }
    for (size_t i = 0; i < pool->thread_count; ++i) {
        pthread_join(pool->threads[i], NULL);
    }
    free(pool->threads);
    pool->threads = NULL;
}
