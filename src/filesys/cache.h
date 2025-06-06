#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include <string.h>

#define BUFFER_CACHE_SIZE 64

struct cache_entry {
  bool valid;  // valid bit
  bool dirty;  // dirty bit
  bool access; // reference bit
  block_sector_t disk_sector;
  uint8_t buffer[BLOCK_SECTOR_SIZE];
};

/* Buffer Caches. */
void cache_init(void);
void cache_close(void);
void cache_read(block_sector_t, void *);
void cache_write(block_sector_t, const void *);

#endif