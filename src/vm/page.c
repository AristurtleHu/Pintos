// #define VM
// #define USERPROG // TODO: Remove this line when finished

#include "vm/page.h"
#include "filesys/filesys.h"
#include "stdbool.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <stdio.h>
#include <string.h>

#define swap_default (size_t)-1

bool load_zero(struct sup_page_table_entry *spte);
bool load_file(struct sup_page_table_entry *spte);

/* Hash less func */
bool page_table_less(const struct hash_elem *a_, const struct hash_elem *b_,
                     void *aux UNUSED) {
  const struct sup_page_table_entry *a =
      hash_entry(a_, struct sup_page_table_entry, elem);
  const struct sup_page_table_entry *b =
      hash_entry(b_, struct sup_page_table_entry, elem);

  return a->uaddr < b->uaddr;
}

/* Hash func */
unsigned page_table_hash(const struct hash_elem *e_, void *aux UNUSED) {
  const struct sup_page_table_entry *e =
      hash_entry(e_, struct sup_page_table_entry, elem);

  return hash_bytes(&e->uaddr, sizeof(e->uaddr));
}

/* Find the page table entry for uaddr. */
void *find_spte(const void *uaddr) {
  struct sup_page_table_entry spte;
  spte.uaddr = pg_round_down(uaddr);

  struct hash_elem *e =
      hash_find(&thread_current()->sup_page_table, &spte.elem);

  return e == NULL ? NULL : hash_entry(e, struct sup_page_table_entry, elem);
}

/* Lazy loads a page from file. */
bool lazy_load(struct file *file, off_t ofs, uint8_t *upage,
               uint32_t page_read_bytes, uint32_t page_zero_bytes,
               bool writable) {
  struct sup_page_table_entry *spte =
      malloc(sizeof(struct sup_page_table_entry));
  if (spte == NULL)
    return false;

  spte->uaddr = upage;
  spte->kaddr = NULL;
  spte->file = file;
  spte->offset = ofs;
  spte->read_bytes = page_read_bytes;
  spte->zero_bytes = page_zero_bytes;
  spte->writable = writable;
  spte->swap_index = swap_default;

  if (page_read_bytes == 0)
    spte->type = ALL_ZERO;
  else
    spte->type = FROM_FILE;

  lock_init(&spte->spte_lock);

  hash_insert(&thread_current()->sup_page_table, &spte->elem);

  return true;
}

/* Stack growth. */
bool stack_grow(void *fault_addr, bool pin) {
  struct sup_page_table_entry *spte =
      malloc(sizeof(struct sup_page_table_entry));
  if (spte == NULL)
    return false;

  spte->uaddr = pg_round_down(fault_addr);
  spte->kaddr = frame_alloc(PAL_USER, spte);

  if (spte->kaddr == NULL) {
    free(spte);
    return false;
  }

  spte->writable = true;
  spte->type = FRAME;
  spte->file = NULL;
  spte->offset = 0;
  spte->read_bytes = 0;
  spte->zero_bytes = PGSIZE;
  spte->swap_index = (size_t)-1;

  lock_init(&spte->spte_lock);

  if (!install_page(spte->uaddr, spte->kaddr, true)) {
    frame_free(spte->kaddr);
    free(spte);
    return false;
  }

  hash_insert(&thread_current()->sup_page_table, &spte->elem);

  pagedir_set_dirty(thread_current()->pagedir, spte->uaddr, true);
  if (!pin)
    frame_unpin(spte->kaddr);

  return true;
}

/* Load a page with all zeroes. */
bool load_zero(struct sup_page_table_entry *spte) {
  spte->kaddr = frame_alloc(PAL_USER | PAL_ZERO, spte);
  if (spte->kaddr == NULL) {
    lock_release(&spte->spte_lock);
    return false;
  }
  return true;
}

/* Load a page from file. */
bool load_file(struct sup_page_table_entry *spte) {
  spte->kaddr = frame_alloc(PAL_USER, spte);
  if (spte->kaddr == NULL) {
    lock_release(&spte->spte_lock);
    return false;
  }

  acquire_file_lock();

  file_seek(spte->file, spte->offset);
  // read bytes from the file
  int read = file_read(spte->file, spte->kaddr, spte->read_bytes);
  if (read != (int)spte->read_bytes) {
    release_file_lock();
    lock_release(&spte->spte_lock);
    frame_free(spte->kaddr);
    return false;
  }

  release_file_lock();

  // zero the rest
  memset(spte->kaddr + spte->read_bytes, 0, spte->zero_bytes);
  return true;
}

/* Load a page from swap or file. */
bool load_page(void *fault_addr, bool pin) {
  struct sup_page_table_entry *spte = find_spte(fault_addr);
  if (spte == NULL)
    return false;

  lock_acquire(&spte->spte_lock);

  // already loaded
  if (spte->kaddr != NULL) {
    if (pin)
      frame_pin(spte->kaddr);
    lock_release(&spte->spte_lock);
    return true;
  }

  // need swap load
  else if (spte->swap_index != swap_default) {
    spte->kaddr = frame_alloc(PAL_USER, spte);
    if (spte->kaddr == NULL)
      return false;

    swap_in(spte->swap_index, spte->kaddr);
    spte->swap_index = swap_default;
  }

  // need file load (all zero)
  else if (spte->type == ALL_ZERO) {
    if (!load_zero(spte))
      return false;
  }

  // need file load
  else {
    if (!load_file(spte))
      return false;
  }

  // map
  if (!install_page(spte->uaddr, spte->kaddr, spte->writable)) {
    lock_release(&spte->spte_lock);
    frame_free(spte->kaddr);
    return false;
  }

  if (!pin)
    frame_unpin(spte->kaddr);

  lock_release(&spte->spte_lock);

  return true;
}

hash_action_func process_free_page;

/* Free the page table entry. */
void process_free_page(struct hash_elem *e, void *aux UNUSED) {
  struct sup_page_table_entry *spte =
      hash_entry(e, struct sup_page_table_entry, elem);

  if (spte->swap_index != swap_default)
    swap_free(spte->swap_index);

  free(spte);
}

/* Free the page table. */
void page_table_free(struct hash *spt) { hash_destroy(spt, process_free_page); }

/* Free all memory mapped files. */
void mmap_files_free(struct list *mmap_list) {
  while (!list_empty(mmap_list)) {
    struct mmap_file *mmap_file =
        list_entry(list_begin(mmap_list), struct mmap_file, elem);
    munmap(mmap_file->mapid);
  }
}