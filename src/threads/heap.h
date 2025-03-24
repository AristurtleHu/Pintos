#ifndef THREADS_HEAP_H
#define THREADS_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Choose by myself */
#define MAX_HEAP_SIZE 2048

typedef void *heap_elem;

/* Return true if A is less than B */
typedef bool heap_less_func(const heap_elem a, const heap_elem b);

struct heap {
  size_t size;                    /* Heap size. */
  heap_elem elems[MAX_HEAP_SIZE]; /* Heap elements. */
  heap_less_func *less;           /* Heap compare function. */
};

void heap_init(struct heap *, heap_less_func *);
void heap_rebuild(struct heap *);

heap_elem heap_top(struct heap *);
void heap_push(struct heap *, heap_elem);
heap_elem heap_pop(struct heap *);

size_t heap_size(struct heap *);
bool heap_empty(struct heap *);

#endif /* threads/heap.h */