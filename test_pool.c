#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
int task_counter = 0;

// Simple task that increments a counter
void increment_task(void* arg) {
    int* value = (int*)arg;
    pthread_mutex_lock(&counter_lock);
    task_counter += *value;
    pthread_mutex_unlock(&counter_lock);
    free(arg);
}

// Task that prints a message
void print_task(void* arg) {
    char* msg = (char*)arg;
    printf("Task executing: %s\n", msg);
}

// Task that sleeps (for testing shutdown)
void sleep_task(void* arg) {
    int* duration = (int*)arg;
    printf("Sleeping for %d seconds...\n", *duration);
    sleep(*duration);
    printf("Done sleeping %d seconds\n", *duration);
    free(arg);
}

void sleep_task_no_free(void* arg) {
    int duration = *(int*)arg;
    printf("Sleeping for %d seconds...\n", duration);
    sleep(duration);
    printf("Done sleeping %d seconds\n", duration);
    // No free() - arg is stack-allocated
}

void test_basic_execution() {
    printf("\n=== Test 1: Basic Execution ===\n");
    Pool* pool = Pool_new(2, 10, POOL_SHUTDOWN_GRACEFUL);
    
    // Submit 10 tasks
    for (int i = 0; i < 10; i++) {
        int* val = malloc(sizeof(int));
        *val = 1;
        Pool_submit(pool, increment_task, val);
    }
    
    Pool_shutdown(pool);
    Pool_free(pool);
    
    printf("Expected counter: 10, Actual: %d\n", task_counter);
    printf("Test %s\n", task_counter == 10 ? "PASSED" : "FAILED");
    task_counter = 0;
}

void test_multiple_threads() {
    printf("\n=== Test 2: Multiple Threads (100 tasks) ===\n");
    Pool* pool = Pool_new(4, 50, POOL_SHUTDOWN_GRACEFUL);
    
    for (int i = 0; i < 100; i++) {
        int* val = malloc(sizeof(int));
        *val = 1;
        Pool_submit(pool, increment_task, val);
    }
    
    Pool_shutdown(pool);
    Pool_free(pool);
    
    printf("Expected counter: 100, Actual: %d\n", task_counter);
    printf("Test %s\n", task_counter == 100 ? "PASSED" : "FAILED");
    task_counter = 0;
}

void test_graceful_shutdown() {
    printf("\n=== Test 3: Graceful Shutdown ===\n");
    Pool* pool = Pool_new(2, 5, POOL_SHUTDOWN_GRACEFUL);
    
    // Submit slow tasks
    for (int i = 0; i < 4; i++) {
        int* duration = malloc(sizeof(int));
        *duration = 1;
        Pool_submit(pool, sleep_task, duration);
    }
    
    printf("Shutting down gracefully (should wait for all tasks)...\n");
    Pool_shutdown(pool);
    printf("Shutdown complete - all tasks finished\n");
    Pool_free(pool);
    printf("Test PASSED (if all 4 sleep tasks completed)\n");
}

void test_immediate_shutdown() {
    printf("\n=== Test 4: Immediate Shutdown ===\n");
    Pool* pool = Pool_new(2, 10, POOL_SHUTDOWN_IMMEDIATE);
    
    int duration = 2;  // Stack variable - no malloc needed
    
    // Submit many slow tasks
    for (int i = 0; i < 10; i++) {
        Pool_submit(pool, sleep_task_no_free, &duration);
    }
    
    sleep(1);  // Let a couple tasks start
    printf("Shutting down immediately (should stop after current tasks)...\n");
    Pool_shutdown(pool);
    printf("Shutdown complete\n");
    Pool_free(pool);
    printf("Test PASSED (if shutdown was quick, not all tasks completed)\n");
}

void test_submit_after_shutdown() {
    printf("\n=== Test 5: Submit After Shutdown ===\n");
    Pool* pool = Pool_new(2, 5, POOL_SHUTDOWN_GRACEFUL);
    
    Pool_shutdown(pool);
    
    int* val = malloc(sizeof(int));
    *val = 1;
    int result = Pool_submit(pool, increment_task, val);
    
    printf("Submit after shutdown returned: %d (expected -1)\n", result);
    printf("Test %s\n", result == -1 ? "PASSED" : "FAILED");
    
    if (result == -1) free(val);  // Clean up if rejected
    Pool_free(pool);
}

int main() {
    printf("Starting Thread Pool Tests\n");
    
    test_basic_execution();
    test_multiple_threads();
    test_graceful_shutdown();
    test_immediate_shutdown();
    test_submit_after_shutdown();
    
    pthread_mutex_destroy(&counter_lock);
    printf("\n=== All tests complete ===\n");
    return 0;
}