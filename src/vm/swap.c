#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>

/* Swap block */
struct block *swap_block;

/* Swap bitmap. */
struct bitmap *swap_bitmap;

/* Swap lock. */
struct lock swap_lock;

/* Number of sectors per page. */
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* Initalize swapping. */
void swap_init(void) {
  swap_block = block_get_role(BLOCK_SWAP);
  swap_bitmap = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE);
  bitmap_set_all(swap_bitmap, false);
  lock_init(&swap_lock);
}

/* Swap in a page. */
void swap_in(size_t swap_index, void *page) {
  lock_acquire(&swap_lock);

  /* Read data. */
  for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
    block_read(swap_block, swap_index * SECTORS_PER_PAGE + i,
               page + i * BLOCK_SECTOR_SIZE);

  bitmap_set(swap_bitmap, swap_index, false);

  lock_release(&swap_lock);
}

/* Swap out a page. */
size_t swap_out(void *page) {
  lock_acquire(&swap_lock);

  /* Find a free swap slot. */
  size_t swap_index = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);

  /* Write data. */
  for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
    block_write(swap_block, swap_index * SECTORS_PER_PAGE + i,
                page + i * BLOCK_SECTOR_SIZE);

  lock_release(&swap_lock);

  return swap_index;
}

/* Free a swap slot.*/
void swap_free(size_t swap_index) {
  lock_acquire(&swap_lock);

  bitmap_set(swap_bitmap, swap_index, false);

  lock_release(&swap_lock);
}