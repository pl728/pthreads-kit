#include "blocking_queue.h"
#include "thread_pool.h"
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

struct Pool {
    Queue *queue;
    size_t pool_size;
    pthread_t *workers;
    PoolShutdownMode shutdown_mode;
    int shutdown_flag;
    pthread_mutex_t mutex;
};

typedef struct {
    void (*function)(void*);
    void* args;
} Task;

void* worker_thread(void* arg) {
    Pool *pool = (Pool*) arg;

    while(1) {
        void *item;
        int rc = Queue_pop(pool->queue, &item);
        if(rc == -1) {
            break;
        }

        Task *t = (Task*) item;
        t->function(t->args);

        free(t);

        if(pool->shutdown_mode == POOL_SHUTDOWN_IMMEDIATE) {
            pthread_mutex_lock(&pool->mutex);
            int should_exit = pool->shutdown_flag;
            pthread_mutex_unlock(&pool->mutex);
            if (should_exit) break;
        }
    }

    return NULL;
}

// constructor
Pool* Pool_new(size_t pool_size, size_t queue_capacity, PoolShutdownMode shutdown_mode) {
    Pool *pool = malloc(sizeof *pool);
    if (!pool) return NULL;

    pool->pool_size = pool_size;
    pool->shutdown_mode = shutdown_mode;
    pool->shutdown_flag = 0;

    pool->queue = Queue_new(queue_capacity);
    if(!pool->queue) {
        free(pool);
        return NULL;
    }

    pool->workers = malloc(pool_size * sizeof(pthread_t));
    if(!pool->workers) {
        Queue_free(pool->queue);
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->mutex, NULL);

    for(size_t i = 0; i < pool_size; i++) {
        pthread_create(&pool->workers[i], NULL, worker_thread, pool);
    }

    return pool;
}

int Pool_submit(Pool* pool, void (*function)(void*), void* args) {
    pthread_mutex_lock(&pool->mutex);

    if(pool->shutdown_flag == 1) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    pthread_mutex_unlock(&pool->mutex);

    Task *t = malloc(sizeof(Task));
    if(!t) return -1;

    t->function = function;
    t->args = args;

    int result = Queue_push(pool->queue, t);
    if(result != 0) {
        free(t);
        return -1;
    }

    return 0;
}

void Pool_shutdown(Pool* pool) {
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown_flag = 1;
    pthread_mutex_unlock(&pool->mutex);

    Queue_shutdown(pool->queue);

    for(size_t i = 0; i < pool->pool_size; i++) {
        pthread_join(pool->workers[i], NULL);
    }

    if (pool->shutdown_mode == POOL_SHUTDOWN_IMMEDIATE) {
        void* item;
        while (Queue_pop(pool->queue, &item) == 0) {
            Task* t = (Task*)item;
            free(t);
        }
    }
}

// destructor
void Pool_free(Pool* pool) {
    if(!pool) return;
    pthread_mutex_destroy(&pool->mutex);
    Queue_free(pool->queue);
    free(pool->workers);
    free(pool);
}
