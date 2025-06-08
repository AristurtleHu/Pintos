#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <list.h>
#include <string.h>

/* Buffer cache. */
static struct cache_entry cache[BUFFER_CACHE_SIZE];

/* A global lock for sync. */
static struct lock cache_lock;

/* Read-ahead variables. */
struct list read_ahead_list;
struct semaphore read_ahead_sema;
struct read_ahead_entry {
  struct list_elem elem; // The list element
  block_sector_t sector; // Sector id
};

static void read_ahead_add(block_sector_t sector);

/* Initialize the buffer cache. */
void cache_init(void) {
  lock_init(&cache_lock);
  for (size_t i = 0; i < BUFFER_CACHE_SIZE; ++i)
    cache[i].valid = false;

  sema_init(&read_ahead_sema, 0);
  list_init(&read_ahead_list);
}

/* Write back a dirty cache entry to disk. */
static void write_back(struct cache_entry *entry) {
  if (entry->dirty) {
    block_write(fs_device, entry->disk_sector, entry->buffer);
    entry->dirty = false;
  }
}

/* Write back all valid cache entries and close the cache. */
void cache_close(void) {
  lock_acquire(&cache_lock);

  for (size_t i = 0; i < BUFFER_CACHE_SIZE; ++i)
    if (cache[i].valid)
      write_back(&cache[i]);

  while (!list_empty(&read_ahead_list)) {
    struct list_elem *e = list_pop_front(&read_ahead_list);
    struct read_ahead_entry *top = list_entry(e, struct read_ahead_entry, elem);
    free(top);
  }

  lock_release(&cache_lock);
}

/* Find a cache entry by disk sector. */
static struct cache_entry *find_cache(block_sector_t sector) {
  for (size_t i = 0; i < BUFFER_CACHE_SIZE; ++i)
    if (cache[i].valid && cache[i].disk_sector == sector)
      return &cache[i];

  return NULL;
}

/* Evict a cache entry using the clock algorithm. */
static struct cache_entry *cache_evict(void) {
  static size_t clock = 0;
  while (true) {
    struct cache_entry *entry = &cache[clock];
    if (!entry->valid)
      return entry;

    if (entry->access)
      entry->access = false;
    else {
      if (entry->dirty)
        write_back(entry);
      entry->valid = false;
      return entry;
    }
    clock = (clock + 1) % BUFFER_CACHE_SIZE;
  }
}

#define EVICT                                                                  \
  if (!entry) {                                                                \
    entry = cache_evict();                                                     \
    entry->valid = true;                                                       \
    entry->disk_sector = sector;                                               \
    entry->dirty = false;                                                      \
    block_read(fs_device, sector, entry->buffer);                              \
  }

/* Read a block from the cache. */
void cache_read(block_sector_t sector, void *mem) {
  lock_acquire(&cache_lock);
  struct cache_entry *entry = find_cache(sector);
  EVICT

  entry->access = true;
  memcpy(mem, entry->buffer, BLOCK_SECTOR_SIZE);

  read_ahead_add(sector + 1); // Read ahead for the next sector

  lock_release(&cache_lock);
}

/* Write a block to the cache. */
void cache_write(block_sector_t sector, const void *data) {
  lock_acquire(&cache_lock);
  struct cache_entry *entry = find_cache(sector);
  EVICT

  entry->access = true;
  entry->dirty = true;
  memcpy(entry->buffer, data, BLOCK_SECTOR_SIZE);
  lock_release(&cache_lock);
}

/* Read ahead wait list addition */
static void read_ahead_add(block_sector_t sector) {
  struct read_ahead_entry *e = malloc(sizeof(struct read_ahead_entry));
  e->sector = sector;
  list_push_back(&read_ahead_list, &e->elem);
  sema_up(&read_ahead_sema);
}

/* Read ahead for the next sector. */
void read_ahead(block_sector_t sector) {
  struct cache_entry *entry = find_cache(sector);
  EVICT

  entry->access = true;
  return;
}