#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int mapid_t;

typedef struct {
  mapid_t id;            /* Memory map id */
  void *addr;            /* Virtual address of mmap */
  struct file *file;     /* Mapped file */
  int page_count;        /* Number of page mapped from file */
  struct list_elem elem; /* Elem for list sturct */
} mmap_entry_t;

void syscall_init(void);

void exit(int);
void munmap(mapid_t);

#endif /* userprog/syscall.h */
