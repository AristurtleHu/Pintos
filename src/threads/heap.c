#include "threads/heap.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void heap_init(struct thread_heap *heap, heap_less_func *less) {
  heap->size = 0;
  heap->capacity = 8;
  heap->threads =
      (struct thread **)malloc(heap->capacity * sizeof(struct thread *));
  heap->less = less;
}

void heap_thread_swap(struct thread **a, struct thread **b) {
  struct thread *temp = *a;
  *a = *b;
  *b = temp;
}

void heap_push(struct thread_heap *heap, struct thread *thread) {
  // size *= 2 each time
  if (heap->size == heap->capacity) {
    heap->capacity *= 2;
    heap->threads = (struct thread **)realloc(
        heap->threads, heap->capacity * sizeof(struct thread *));
  }

  heap->threads[heap->size] = thread;

  int i = heap->size;
  while (i > 0 && heap->less(heap->threads[i], heap->threads[(i - 1) / 2])) {
    heap_thread_swap(&heap->threads[i], &heap->threads[(i - 1) / 2]);
    i = (i - 1) / 2;
  }

  heap->size++;
}

struct thread *heap_pop(struct thread_heap *heap) {
  if (heap->size == 0)
    return NULL;

  struct thread *result = heap->threads[0];
  heap->size--;
  heap->threads[0] = heap->threads[heap->size];

  int i = 0;
  while (2 * i + 1 < heap->size) {
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    int smallest = left;
    if (right < heap->size &&
        heap->less(heap->threads[right], heap->threads[left])) {
      smallest = right;
    }

    // The heap property is satisfied
    if (heap->less(heap->threads[i], heap->threads[smallest]))
      break;

    heap_thread_swap(&heap->threads[i], &heap->threads[smallest]);
    i = smallest;
  }

  return result;
}

struct thread *heap_top(struct thread_heap *heap) {
  return heap->size == 0 ? NULL : heap->threads[0];
}

bool heap_empty(struct thread_heap *heap) { return heap->size == 0; }