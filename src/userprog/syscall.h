#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int mapid_t;

void syscall_init(void);

void exit(int);
void munmap(mapid_t);

#endif /* userprog/syscall.h */
