#include "heap.h"
#include <stdio.h>

/* Swap two elements, helper function */
static void swap(heap_elem *a, heap_elem *b) {
  heap_elem t = *a;
  *a = *b;
  *b = t;
}

/* Initialize heap */
void heap_init(struct heap *heap, heap_less_func *less) {
  ASSERT(heap != NULL);

  heap->size = 0;
  heap->less = less;
}

/* Rebuild heap to fit donated priority */
void heap_rebuild(struct heap *heap) {
  ASSERT(heap != NULL);

  for (size_t i = heap->size / 2; i >= 1; i--) {
    size_t index = i;

    while (2 * index <= heap->size) {
      size_t child = 2 * index;

      if (child < heap->size &&
          heap->less(heap->elems[child], heap->elems[child + 1]))
        child++;

      if (heap->less(heap->elems[index], heap->elems[child])) {
        swap(&heap->elems[index], &heap->elems[child]);
        index = child;
      } else {
        break;
      }
    }
  }
}

/* Return the top element */
heap_elem heap_top(struct heap *heap) {
  return heap->size > 0 ? heap->elems[1] : NULL;
}

/* Push element to heap */
void heap_push(struct heap *heap, heap_elem elem) {
  ASSERT(heap->size + 1 <= MAX_HEAP_SIZE);

  heap->size++;
  heap->elems[heap->size] = elem;

  size_t index = heap->size;
  while (index > 1) {
    size_t parent = index / 2;

    if (heap->less(heap->elems[parent], heap->elems[index])) {
      swap(&heap->elems[parent], &heap->elems[index]);
      index = parent;
    } else {
      break;
    }
  }
}

/* Pop element from heap */
heap_elem heap_pop(struct heap *heap) {
  ASSERT(!heap_empty(heap));

  heap_elem result = heap->elems[1];
  heap->elems[1] = heap->elems[heap->size];
  heap->size--;

  size_t index = 1;
  while (2 * index <= heap->size) {
    size_t child = 2 * index;

    if (child < heap->size &&
        heap->less(heap->elems[child], heap->elems[child + 1]))
      child++;

    if (heap->less(heap->elems[index], heap->elems[child])) {
      swap(&heap->elems[index], &heap->elems[child]);
      index = child;
    } else {
      break;
    }
  }

  return result;
}

/* Return heap size */
size_t heap_size(struct heap *heap) { return heap->size; }

/* Return true if heap is empty */
bool heap_empty(struct heap *heap) { return heap->size == 0; }