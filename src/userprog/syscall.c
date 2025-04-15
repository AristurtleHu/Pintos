#define USERPROG // TODO: Remove this line when you finish the project

#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <syscall-nr.h>

static void syscall_handler(struct intr_frame *);
static int get_user(const uint8_t *uaddr);

void syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_address(const void *addr) {
  // Check if the address is within the user address space
  if (!is_user_vaddr(addr)) {
    thread_current()->exit_code = -1;
    thread_exit();
  }

  // Check if the address is not NULL
  void *ptr = pagedir_get_page(thread_current()->pagedir, addr);
  if (!ptr) {
    thread_current()->exit_code = -1;
    thread_exit();
  }

  uint8_t *check_by = (uint8_t *)addr;

  for (uint8_t i = 0; i < 4; i++) {
    if (get_user(check_by + i) == -1) {
      thread_current()->exit_code = -1;
      thread_exit();
    }
  }
}

static int get_user(const uint8_t *uaddr) {
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:" : "=&a"(result) : "m"(*uaddr));
  return result;
}

static void syscall_handler(struct intr_frame *f UNUSED) {
  printf("system call!\n");
  thread_exit();
}
