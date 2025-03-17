#include "threads/heap.h"
#include "threads/thread.h"

void heap_push(struct thread_heap *heap, struct thread *thread) {
  // FIXME: implement heap_push
  if (heap->size == heap->capacity) {
    heap->capacity *= 2;
    heap->threads = (struct thread **)realloc(
        heap->threads, heap->capacity * sizeof(struct thread *));
  }
  heap->threads[heap->size] = thread;
  int i = heap->size;
  while (i > 0 && heap->threads[i]->wakeup_time <
                      heap->threads[(i - 1) / 2]->wakeup_time) {
    struct thread *temp = heap->threads[i];
    heap->threads[i] = heap->threads[(i - 1) / 2];
    heap->threads[(i - 1) / 2] = temp;
    i = (i - 1) / 2;
  }
  heap->size++;
}