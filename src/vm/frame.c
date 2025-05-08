// #define VM
// #define USERPROG // TODO: Remove this line when finished

#include "vm/frame.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* List of allocated frames. */
struct list frame_table;

/* Lock for frame table. */
struct lock frame_lock;

/* clock_ptr for clock. */
struct list_elem *clock_ptr;

void *evict_frame(void);
bool all_pinned(void);
struct frame_table_entry *clock_next(void);
void write_file(struct frame_table_entry *fte);

/* Initialize the frame table. */
void frame_init(void) {
  list_init(&frame_table);
  lock_init(&frame_lock);
  clock_ptr = list_begin(&frame_table);
}

/* Find the frame table entry for the given kernel address KADDR.
   Return NULL if not found. */
struct frame_table_entry *find_frame(void *kaddr) {
  struct frame_table_entry *fte = NULL;

  for (struct list_elem *e = list_begin(&frame_table);
       e != list_end(&frame_table); e = list_next(e)) {

    fte = list_entry(e, struct frame_table_entry, elem);

    if (fte->kaddr == kaddr)
      return fte;
  }

  return NULL;
}

/* Find the next frame table entry in the clock algorithm. */
struct frame_table_entry *clock_next(void) {
  struct frame_table_entry *fte = NULL;

  if (clock_ptr == list_end(&frame_table))
    clock_ptr = list_begin(&frame_table);

  fte = list_entry(clock_ptr, struct frame_table_entry, elem);
  clock_ptr = list_next(clock_ptr);

  return fte;
}

/* Allocate a frame to SPTE.
   Remember: Call install_page() after this function. */
void *frame_alloc(enum palloc_flags flags, struct sup_page_table_entry *spte) {
  lock_acquire(&frame_lock);

  void *kaddr = palloc_get_page(flags);
  if (kaddr == NULL) {
    kaddr = evict_frame();

    if (kaddr == NULL) { // all pinned
      lock_release(&frame_lock);
      return NULL;
    }
  }

  if (flags & PAL_ZERO) // copyed from palloc.c
    memset(kaddr, 0, PGSIZE);
  if (flags & PAL_ASSERT)
    PANIC("palloc_get: out of pages");

  struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
  if (fte == NULL) {
    palloc_free_page(kaddr);
    lock_release(&frame_lock);
    return NULL;
  }

  fte->kaddr = kaddr;
  fte->spte = spte;
  fte->owner = thread_current();
  fte->pinned = true; // cannot evict now
  list_push_back(&frame_table, &fte->elem);

  lock_release(&frame_lock);
  return kaddr;
}

/* Free the frame belongs to KADDR. */
void frame_free(void *kaddr) {
  struct frame_table_entry *fte = find_frame(kaddr);

  if (fte == NULL)
    return;

  lock_acquire(&frame_lock);

  if (clock_ptr == &fte->elem)
    clock_ptr = list_next(clock_ptr);

  list_remove(&fte->elem);
  palloc_free_page(kaddr);
  free(fte);

  lock_release(&frame_lock);
}

/* Remove all frames belongs to thread T */
void frame_remove(struct thread *t) {
  struct frame_table_entry *fte = NULL;

  lock_acquire(&frame_lock);

  for (struct list_elem *e = list_begin(&frame_table), *next;
       e != list_end(&frame_table); e = next) {
    next = list_next(e);

    fte = list_entry(e, struct frame_table_entry, elem);
    if (fte->owner == t) {

      if (clock_ptr == &fte->elem)
        clock_ptr = next;

      e = list_remove(&fte->elem);
      free(fte);
    }
  }

  lock_release(&frame_lock);
}

/* Pin the frame (no evict) */
void frame_pin(void *kaddr) {
  struct frame_table_entry *fte = find_frame(kaddr);

  if (fte == NULL)
    return;

  lock_acquire(&frame_lock);
  fte->pinned = true;
  lock_release(&frame_lock);
}

/* Unpin the frame (can evict) */
void frame_unpin(void *kaddr) {
  struct frame_table_entry *fte = find_frame(kaddr);

  if (fte == NULL)
    return;

  lock_acquire(&frame_lock);
  fte->pinned = false;
  lock_release(&frame_lock);
}

/* Check if all frames are pinned. */
bool all_pinned(void) {
  struct frame_table_entry *fte = NULL;

  for (struct list_elem *e = list_begin(&frame_table);
       e != list_end(&frame_table); e = list_next(e)) {
    fte = list_entry(e, struct frame_table_entry, elem);

    if (!fte->pinned)
      return false;
  }

  return true;
}

/* Write for mmap. */
void write_file(struct frame_table_entry *fte) {
  struct file *file = fte->spte->file;

  acquire_file_lock();
  file_seek(file, fte->spte->offset);
  file_write(file, fte->kaddr, PGSIZE);
  release_file_lock();

  pagedir_set_dirty(fte->owner->pagedir, fte->spte->uaddr, false);
}

/* Evict a frame and return kaddr. */
void *evict_frame(void) {
  struct frame_table_entry *fte = NULL;
  bool frame_available = !all_pinned();

  while (frame_available) {
    fte = clock_next();
    if (!fte->pinned) {

      // A second chance, the clock hand moves on.
      if (pagedir_is_accessed(fte->owner->pagedir, fte->spte->uaddr))
        pagedir_set_accessed(fte->owner->pagedir, fte->spte->uaddr, false);

      else {
        enum intr_level old_level = intr_disable(); // critical section
        lock_acquire(&fte->spte->spte_lock);

        fte->spte->kaddr = NULL;
        pagedir_clear_page(fte->owner->pagedir, fte->spte->uaddr);

        list_remove(&fte->elem);
        intr_set_level(old_level);
        break;
      }
    }
  }

  // Done: Get the frame table entry.

  if (fte == NULL)
    return NULL;

  if (fte->spte->type == PAGE_MMAP &&
      pagedir_is_dirty(fte->owner->pagedir, fte->spte->uaddr))
    write_file(fte);

  else
    fte->spte->swap_index = swap_out(fte->kaddr);

  lock_release(&fte->spte->spte_lock);

  void *kaddr = fte->kaddr;
  free(fte);

  return kaddr;
}
