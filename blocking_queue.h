#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

// opaque struct
typedef struct Queue Queue;

// constructor
Queue* Queue_new(int capacity);

// operations
int Queue_push(Queue* q, void* item);
int Queue_pop(Queue* q, void** out);
void Queue_shutdown(Queue* q);

// destructor
void Queue_free(Queue* q);

#endif