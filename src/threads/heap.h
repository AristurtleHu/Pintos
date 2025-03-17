#ifndef THREADS_HEAP_H
#define THREADS_HEAP_H

struct thread_heap {
  // FIXME: implement thread_heap
  struct thread **threads;
  int size;
  int capacity;
};

void heap_push(struct thread_heap *heap, struct thread *thread);

#endif /* threads/heap.h */