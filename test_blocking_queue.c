#include "blocking_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_ITEMS 100000

void* producer_a(void* arg) {
    Queue *q = (Queue*) arg;
    for(int i = 0; i < NUM_ITEMS; i++) {
        int *item = malloc(sizeof *item);
        if(!item) break;

        *item = i;

        if(Queue_push(q, item) != 0) {
            free(item);
            break;
        }
    }

    return NULL;
}

void* consumer_a(void* arg) {
    Queue *q = (Queue*) arg;
    int *successes = malloc(sizeof *successes);
    if(!successes) return NULL;
    *successes = 0;

    for(int i = 0; i < NUM_ITEMS; i++) {
        void *result;
        if(Queue_pop(q, &result) != 0) {
            break;
        }

        (*successes)++;
        free(result);
    }

    return successes;
}

void test_single_producer_consumer() {
 // one producer, one consumer, NUM_ITEMS items
    printf("Running test: Single producer/single consumer (%d items)...\n", NUM_ITEMS);
    Queue *q = Queue_new(10);
    if(!q) {
        printf("Failed to create queue");
        return;
    }

    pthread_t producer_thread;
    pthread_t consumer_thread;

    int rc = pthread_create(&producer_thread, NULL, producer_a, q);
    if(rc != 0) {
        printf("[Error] Failed to create producer thread (rc=%d)\n", rc);
        Queue_free(q);
        return;
    }

    rc = pthread_create(&consumer_thread, NULL, consumer_a, q);
    if(rc != 0) {
        printf("[Error] Failed to create consumer thread (rc=%d)\n", rc);
        Queue_shutdown(q);
        pthread_join(producer_thread, NULL);
        Queue_free(q);
        return;
    }

    pthread_join(producer_thread, NULL);

    void* consumer_ret;
    pthread_join(consumer_thread, &consumer_ret);

    if(consumer_ret) {
        int pops = *(int*)consumer_ret;
        free(consumer_ret);
        if(pops != NUM_ITEMS) {
            printf("[FAILURE] Expected %d items, got %d\n", NUM_ITEMS, pops);
        } else {
            printf("[SUCCESS] Successfully processed %d items\n", NUM_ITEMS);
        }
    } else {
        printf("[FAILURE] Consumer allocation failed or returned NULL\n");
    }

    Queue_free(q);
    printf("=========================================================\n\n");
}

void test_multi_producer_consumer() {
    printf("=== Test: Multi Producer/Consumer (5p/5c, %d items each) ===\n", NUM_ITEMS);
    
    const int NUM_PRODUCERS = 5;
    const int NUM_CONSUMERS = 5;
    
    Queue *q = Queue_new(10);
    if(!q) {
        printf("Failed to create queue\n");
        return;
    }

    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];

    for(int i = 0; i < NUM_PRODUCERS; i++) {
        int rc = pthread_create(&producers[i], NULL, producer_a, q);
        if(rc != 0) {
            printf("[Error] Failed to create producer %d (rc=%d)\n", i, rc);
            Queue_free(q);
            return;
        }
    }

    for(int i = 0; i < NUM_CONSUMERS; i++) {
        int rc = pthread_create(&consumers[i], NULL, consumer_a, q);
        if(rc != 0) {
            printf("[Error] Failed to create consumer %d (rc=%d)\n", i, rc);
            Queue_free(q);
            return;
        }
    }

    for(int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    int total_consumed = 0;
    for(int i = 0; i < NUM_CONSUMERS; i++) {
        void* consumer_ret;
        pthread_join(consumers[i], &consumer_ret);
        
        if(consumer_ret) {
            int pops = *(int*)consumer_ret;
            total_consumed += pops;
            free(consumer_ret);
        }
    }

    int expected = NUM_PRODUCERS * NUM_ITEMS;
    if(total_consumed != expected) {
        printf("[FAILURE] Expected %d items, got %d\n", expected, total_consumed);
    } else {
        printf("[SUCCESS] Successfully processed %d items\n", total_consumed);
    }

    Queue_free(q);
    printf("=========================================================\n\n");
}

int main() {
    test_single_producer_consumer();
    test_multi_producer_consumer();
    return 0;
    
}