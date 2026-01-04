#include "blocking_queue.h"
#include <stdlib.h>
#include <pthread.h>

struct Queue {
    void** items;
    int head, tail, count, capacity;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    int shutdown_flag;
};

// constructor
Queue* Queue_new(int capacity) {
    if(capacity <= 0) return NULL;
    Queue *q = malloc(sizeof *q);
    if(!q) return NULL;

    q->items = malloc((size_t) capacity * sizeof *q->items);
    if(!q->items){
        free(q);
        return NULL;
    }
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->capacity = capacity;
    q->shutdown_flag = 0;

    if(pthread_mutex_init(&q->mutex, NULL) != 0) {
        free(q->items);
        free(q);
        return NULL;
    }

    if(pthread_cond_init(&q->not_full, NULL) != 0) {
        pthread_mutex_destroy(&q->mutex);
        free(q->items);
        free(q);
        return NULL;
    }

    if(pthread_cond_init(&q->not_empty, NULL) != 0) {
        pthread_cond_destroy(&q->not_full);
        pthread_mutex_destroy(&q->mutex);
        free(q->items);
        free(q);
        return NULL;
    }

    return q;
}

// operations
int Queue_push(Queue* q, void* item) {
    if (!q) return -1;

    pthread_mutex_lock(&q->mutex);
    while(q->count == q->capacity && !q->shutdown_flag) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    if(q->shutdown_flag) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    q->items[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

int Queue_pop(Queue* q, void** out) {
    if (!q || !out) return -1;

    pthread_mutex_lock(&q->mutex);
    while(q->count == 0 && !q->shutdown_flag) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if(q->shutdown_flag && q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    *out = q->items[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

void Queue_shutdown(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    q->shutdown_flag = 1;

    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    
    pthread_mutex_unlock(&q->mutex);
}

// destructor
void Queue_free(Queue* q) {
    if(!q) return;
    
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    pthread_mutex_destroy(&q->mutex);

    free(q->items);
    free(q);
}