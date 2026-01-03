### Preliminary Summary

**Organization:**
We're using the opaque struct pattern with `.h` and `.c` files. The header declares `typedef struct Q Q` so the struct internals stay hidden.

**Initialization:**
The queue has a fixed capacity, so `new()` takes a capacity parameter. Inside `new()`, we malloc memory for the Q struct itself, then create a heap-allocated array with `malloc(sizeof(void*) * capacity)` to hold the generic pointers.

**Circular Buffer Implementation:**
For `push()` and `pop()`, we implement a circular buffer backed by the fixed array. We track `head` and `tail` indices, both initialized to 0. When we push, we write to where `tail` points and increment `tail`. When we pop, we return the item at `head` and increment `head`. Both wrap around the array.

The wrap-around logic is tricky. Suppose `head` points at index 0 and `tail` points at n-1 (the last slot). If we push an item, `tail` would increment to 0, making `head == tail`. 

We need an invariant: only `head` incrementing (via `pop()`) or calling `new()` can cause `head == tail` from the empty state. When pushing, `tail` can increment up to but not past `head` when full. When popping, `head` should never increment past `tail`.

This gives us two scenarios where `head == tail`: when the queue is empty and when it's full. To distinguish between them, we track `curr_items`. When `head == tail` and `curr_items == 0`, it's empty. When `head == tail` and `curr_items == capacity`, it's full.

**Thread Safety:**
At first glance, this seems similar to unsafe functions like `strtok` that have hidden global state. But we need to ensure that when multiple threads call `pop()` and `push()`, we don't get data races on the Q struct fields (`tail`, `head`, `queue`, `curr_items`, etc.).

The spec also requires that `push()` must block when full and `pop()` must block when empty. We can achieve this with condition variables (see https://diveintosystems.org/book/C14-SharedMemory/other_syncs.html).

In the DIS example, there's a shared variable `num_eggs` and a `pthread_cond_t eggs` condition variable used to block/wake up a consumer waiting for eggs. Since we have both `push` and `pop`, we need 2 `pthread_cond_t` variables: one to wake up push threads (waiting on not-full) and one to wake up pop threads (waiting on not-empty).

We can follow the same pattern to block/wake up threads waiting for generic pointers. Note that the DIS example also has logic for creating and joining threads, but in part A of this project, we're only implementing the Q struct itself. Using condition variables and mutexes, we can make our struct interface thread-safe and handle blocking on full/empty.

**Shutdown:**
The `shutdown()` function should "unblock all waiting threads." At first, this is unclear—what does unblocking mean? On second thought, it means that if threads are blocked waiting in `push()` or `pop()`, calling `shutdown()` should wake them up so they can stop waiting and proceed to join/close, thus shutting down gracefully.

Because we're using `pthread_cond_t`, we need `shutdown()` to call `pthread_cond_broadcast()` to wake up ALL waiting threads. Then those threads can continue and join.

## Blocking Queue Design
### API
```c
// blocking_queue.h

typedef struct Queue Queue;
Queue* Queue_new(int capacity);
void Queue_free(Queue* q);
int Queue_push(Queue* q, void* item);
int Queue_pop(Queue* q, void** out);
void Queue_shutdown(Queue* q);
```

```c
// blocking_queue.c
struct Queue {
    void** items;
    int head, tail, count, capacity;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    int shutdown_flag;
}
```

### Implementation Notes
Initially, my instinct was to use `void* Queue_pop(Queue*);`. However, this wouldnt allow the caller to know whether the call succeeded. The Queue could be in a shutdown state, for example. 

From the specification, we know that a call to Queue_pop could be in one of three states: success, blocked, or shutdown. Blocking doesnt require a different return value - it resolves to either success or shutdown. Thus, a good design might be the following: add `void** item_out` as an argument to `int Queue_pop(..args)`. If we successfully pop, we write to item_out and return 0. Ohterwise, if a call is blocked, and shutdown occurs, we return -1.

`Queue_new()` and `Queue_free()` are part of the opaque struct pattern. In this pattern, a user cannot manually allocate the struct, since it isn't defined in the header file. Thus, the caller is forced to call constructor and destructor.

## Invariants
- Queue capacity is fixed at initialization
- When `count == capacity`, `push()` blocks until `count < capacity`
- After successful `push()`, `count` increases by 1 and returns `0`
- When `count == 0`, `pop()` blocks until `count > 0`
- After successful `pop()`, `count` decreases by 1 and returns `0`
- After `shutdown()`, all blocked threads wake up and return `-1`
- `0 <= count <= capacity` at all times
- `count == 0` implies queue is empty
- `count == capacity` implies queue is full
- `mutex` is held when accessing/modifying any Queue state (`head`, `tail`, `count`, `items[]`, `shutdown_flag`)

## Shutdown Behavior
- `shutdown()` sets `shutdown_flag = 1` and broadcasts to both condition variables `not_empty`, `not_full`
- This causes all blocked threads, either in `push()` or `pop()`, to unblock and wake up
- Immediately after wake, threads check the `shutdown_flag` field, which is now `1` and returns `-1`
- Remaining items in queue are discarded after `shutdown()` is called
