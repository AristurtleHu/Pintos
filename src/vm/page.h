#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/file.h"
#include "threads/synch.h"
#include <debug.h>
#include <hash.h>
#include <stddef.h>

#define MAX_STACK_SIZE (1 << 22) // 4 MB

enum sup_page_type {
  ALL_ZERO,  // Page (all zero)
  FROM_FILE, // Page from filesys
  FRAME,     // Page on frame (user stack)
};

struct sup_page_table_entry {
  void *uaddr;           // User virtual address
  void *kaddr;           // Kernel virtual address
  struct hash_elem elem; // Hash element

  bool writable;           // Is page write or read
  enum sup_page_type type; // Type of page

  // file use
  struct file *file;   // File to load
  off_t offset;        // Offset in file
  uint32_t read_bytes; // Number of bytes to read
  uint32_t zero_bytes; // Number of bytes to zero

  // swap use
  struct lock spte_lock; // Lock for waiting swap
  size_t swap_index;     // Swap index
};

hash_less_func page_table_less;
hash_hash_func page_table_hash;

void *find_spte(const void *uaddr);

bool load_page(void *fault_addr, bool pin);

bool lazy_load(struct file *file, off_t ofs, uint8_t *upage,
               uint32_t page_read_bytes, uint32_t page_zero_bytes,
               bool writable);
bool stack_grow(void *fault_addr, bool pin);

void page_table_free(struct hash *spt);
void mmap_files_free(struct list *mmap_list);

#endif /* vm/page.h */