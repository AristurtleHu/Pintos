#ifndef THREADS_HEAP_H
#define THREADS_HEAP_H

#include "threads/thread.h"

typedef bool heap_less_func(const struct thread *a, const struct thread *b);

struct thread_heap {
  struct thread **threads; // array of threads
  int size;                // how many elements are in the heap
  int capacity;            // how many elements can the heap hold
  heap_less_func *less;    // function to compare two threads
};

/* initialize a heap */
void heap_init(struct thread_heap *heap, heap_less_func *less);

/* swap two threads */
void heap_thread_swap(struct thread **a, struct thread **b);

/* push a thread into the heap */
void heap_push(struct thread_heap *heap, struct thread *thread);

/* pop the top thread from the heap */
struct thread *heap_pop(struct thread_heap *heap);

/* get the top thread of the heap */
struct thread *heap_top(struct thread_heap *heap);

#endif /* threads/heap.h */