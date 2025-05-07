#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/page.h"
#include <list.h>
#include <stddef.h>

struct frame_table_entry {
  void *kaddr;                       // Kernel address
  struct sup_page_table_entry *spte; // Linked supplementary page table entry
  struct thread *owner;              // assciated thread

  bool pinned;           // Used to prevent a frame from being evicted
  struct list_elem elem; // List element
};

void frame_init(void);
void *frame_alloc(enum palloc_flags flags, struct sup_page_table_entry *spte);
void frame_free(void *kaddr);

void frame_remove(struct thread *t);

void frame_pin(void *kaddr);
void frame_unpin(void *kaddr);

#endif /* vm/frame.h */