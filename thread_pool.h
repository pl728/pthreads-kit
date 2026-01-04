#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>

typedef struct Pool Pool;

typedef enum {
    POOL_SHUTDOWN_GRACEFUL = 0,
    POOL_SHUTDOWN_IMMEDIATE = 1,
} PoolShutdownMode;

// constructor
Pool* Pool_new(size_t pool_size, size_t queue_capacity, PoolShutdownMode shutdown_mode);

int Pool_submit(Pool* pool, void (*function)(void*), void* arg);

void Pool_shutdown(Pool* pool);

// destructor
void Pool_free(Pool* pool);

#endif