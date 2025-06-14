// #define USERPROG // TODO: Remove this line when finished
// #define FILESYS  // TODO: Remove this line when finished

#include "userprog/syscall.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "list.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>

#ifdef VM
#include "vm/frame.h"
#endif

static void syscall_handler(struct intr_frame *);
static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *, uint8_t);
static void halt(void);
void exit(int);
static tid_t exec(const char *);
static int wait(tid_t);
static bool create(const char *, unsigned);
static bool remove(const char *);
static int open(const char *);
static int filesize(int);
static int read(int, void *, unsigned);
static int write(int, const void *, unsigned);
static void seek(int, unsigned);
static unsigned tell(int);
static void close(int);

#ifdef VM
static mapid_t mmap(int, void *);
static bool check_overlaps(void *addr, int size);
#endif

static bool chdir(const char *);
static bool mkdir(const char *);
static bool readdir(int, char *);
static bool isdir(int);
static int inumber(int);

/* Find the file based on fd */
static struct thread_file *find_file(int fd) {
  struct thread_file *file = NULL;
  struct list_elem *elem;
  struct list *f = &thread_current()->files;

  for (elem = list_begin(f); elem != list_end(f); elem = list_next(elem)) {
    file = list_entry(elem, struct thread_file, elem);

    if (file->fd == fd)
      return file;
  }
  return NULL;
}

void syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Check address validity */
static void *check_address(const void *addr) {
  // Check if the address is within the user address space
  if (!is_user_vaddr(addr))
    exit(-1);

  // Check if the address is not NULL
  void *ptr = pagedir_get_page(thread_current()->pagedir, addr);
  if (!ptr)
    exit(-1);

  uint8_t *check_by = (uint8_t *)addr;

  // Check each byte of the address
  for (uint8_t i = 0; i < 4; i++) {
    if (get_user(check_by + i) == -1)
      exit(-1);
  }

  return ptr;
}

/* Check if str is a valid string in user space. */
static bool check_str(const char *str, size_t size) {
  const uint8_t *ptr = check_address(str);
  size_t i = 0;

  // Check each byte of the string
  while (i < size) {
    int ch = get_user(ptr + i);

    if (ch == -1)
      exit(-1);
    if (ch == '\0')
      break;

    i++;
  }

  if (i == size)
    return false;
  return true;
}

/* Check if str is able to write */
static void *check_write(void *vaddr, size_t size) {
  if (!is_user_vaddr(vaddr))
    exit(-1);

  // Check each byte is able to write
  for (size_t i = 0; i < size; i++) {
    if (!put_user(vaddr + i, 0))
      exit(-1);
  }
  return vaddr;
}

/* Helper function to get a user byte from the address space. */
static int get_user(const uint8_t *uaddr) {
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:" : "=&a"(result) : "m"(*uaddr));
  return result;
}

/* Helper function to check writes to user address. True for success. */
static bool put_user(uint8_t *udst, uint8_t byte) {
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}

/* Switch to corresponding syscall */
static void syscall_handler(struct intr_frame *f UNUSED) {
  int syscall = *(int *)check_address(f->esp);

#ifdef VM
  thread_current()->esp = f->esp;
#endif

  switch (syscall) {
  case SYS_HALT: {
    halt();
    NOT_REACHED();
  }

  case SYS_EXIT: {
    int status = *(int *)check_address(f->esp + sizeof(int *));
    exit(status);
    NOT_REACHED();
  }

  case SYS_EXEC: {
    const char *cmd_line =
        *(const char **)check_address(f->esp + sizeof(int *));
    f->eax = (uint32_t)exec(cmd_line);
    break;
  }

  case SYS_WAIT: {
    tid_t tid = *(tid_t *)check_address(f->esp + sizeof(int *));
    f->eax = (uint32_t)wait(tid);
    break;
  }

  case SYS_CREATE: {
    const char *file = *(const char **)check_address(f->esp + sizeof(int *));
    unsigned initial_size =
        *(unsigned *)check_address(f->esp + 2 * sizeof(int *));
    f->eax = (uint32_t)create(file, initial_size);
    break;
  }

  case SYS_REMOVE: {
    const char *file = *(const char **)check_address(f->esp + sizeof(int *));
    f->eax = (uint32_t)remove(file);
    break;
  }

  case SYS_OPEN: {
    const char *file = *(const char **)check_address(f->esp + sizeof(int *));
    f->eax = (uint32_t)open(file);
    break;
  }

  case SYS_FILESIZE: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    f->eax = (uint32_t)filesize(fd);
    break;
  }

  case SYS_READ: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    void *buffer = *(void **)check_address(f->esp + 2 * sizeof(int *));
    unsigned size = *(unsigned *)check_address(f->esp + 3 * sizeof(int *));
    f->eax = (uint32_t)read(fd, buffer, size);
    break;
  }

  case SYS_WRITE: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    const void *buffer =
        *(const void **)check_address(f->esp + 2 * sizeof(int *));
    unsigned size = *(unsigned *)check_address(f->esp + 3 * sizeof(int *));
    f->eax = write(fd, buffer, size);
    break;
  }

  case SYS_SEEK: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    unsigned position = *(unsigned *)check_address(f->esp + 2 * sizeof(int *));
    seek(fd, position);
    break;
  }

  case SYS_TELL: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    f->eax = (uint32_t)tell(fd);
    break;
  }

  case SYS_CLOSE: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    close(fd);
    break;
  }

#ifdef VM
  case SYS_MMAP: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    void *addr = *(void **)check_address(f->esp + 2 * sizeof(int *));
    f->eax = (uint32_t)mmap(fd, addr);
    break;
  }

  case SYS_MUNMAP: {
    mapid_t mapping = *(mapid_t *)check_address(f->esp + sizeof(int *));
    munmap(mapping);
    break;
  }
#endif

  case SYS_CHDIR: {
    const char *dir = *(const char **)check_address(f->esp + sizeof(int *));
    f->eax = chdir(dir);
    break;
  }

  case SYS_MKDIR: {
    const char *dir = *(const char **)check_address(f->esp + sizeof(int *));
    f->eax = mkdir(dir);
    break;
  }

  case SYS_READDIR: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    char *name = *(char **)check_address(f->esp + 2 * sizeof(int *));
    f->eax = readdir(fd, name);
    break;
  }

  case SYS_ISDIR: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    f->eax = isdir(fd);
    break;
  }

  case SYS_INUMBER: {
    int fd = *(int *)check_address(f->esp + sizeof(int *));
    f->eax = inumber(fd);
    break;
  }

  default:
    PANIC("Unknown system call.");
  }
}

/* Terminates Pintos by calling shutdown_power_off()
    (declaredin'devices/shutdown.h'). This should be
    seldom used, because you lose some information
    about possible deadlock situations, etc. */
static void halt(void) { shutdown_power_off(); }

/* Terminates the current user program, returning status to the kernel.
    If the process's parent waits for it (see below), this is the status that
    will be returned. Conventionally, a status of 0 indicates success and
    nonzero values indicate errors. */
void exit(int status) {
  thread_current()->exit_code = status;
  thread_exit();
}

/* Writes SIZE bytes from buffer to the open file FD. Returns the
    number of bytes actually written, which may be less than SIZE if
    some bytes could not be written.

    Writing past end-of-file would normally extend the file, but file
    growth is not implemented by the basic file system. The expected
    behavior is to write as many bytes as possible up to end-of-file
    and return the actual number written, or 0 if no bytes could be
    written at all.

    Fd 1 writes to the console. The code to write to the console should
    write all of buffer in one call to putbuf(), at least as long as SIZE
    is not bigger than a few hundred bytes. (It is reasonable to break up
    larger buffers.) Otherwise, lines of text output by different processes
    may end up interleaved on the console, confusing both human readers and
    the grading scripts. */
static int write(int fd, const void *buffer, unsigned size) {
  check_address(buffer + size - 1);

  if (fd == STDOUT) { // fd == 1
    putbuf(buffer, size);
    return size;
  } else {
    struct thread_file *thread_file = find_file(fd);
    if (thread_file == NULL)
      return 0;
    if (isdir(fd))
      return -1;

    acquire_file_lock();
    int bytes_written = file_write(thread_file->file, buffer, size);
    release_file_lock();

    return bytes_written;
  }
}

/* Runs the executable whose name is given in CMD_LINE,
    passing any given arguments, and returns the new
    process’s program id (pid). Must return pid -1,
    which otherwise should not be a valid pid, if
    the program cannot load or run for any reason.
    Thus, the parent process cannot return from the
    exec until it knows whether the child process
    successfully loaded its executable.

    Use appropriate synchronization to ensure this. */
static tid_t exec(const char *cmd_line) {
  if (!check_str(cmd_line, 129))
    return -1;

  return process_execute(cmd_line);
}

/* Waits for a child process PID and retrieves the
    child’s exit status.

    If PID is still alive, waits until it terminates.
    Then, returns the status that PID passed to exit.
    If PID did not call exit(), but was terminated by
    the kernel (e.g. killed due to an exception), wait(PID)
    must return -1. It is perfectly legal for a parent
    process to wait for child processes that have already
    terminated by the time the parent calls wait, but the
    kernel must still allow the parent to retrieve its
    child’s exit status, or learn that the child was
    terminated by the kernel.

    wait must fail and return -1 immediately if any of the
    following conditions is true:

    a. PID does not refer to a direct child of the calling
    process. PID is a direct child of the calling process if
    and only if the calling process received PID as a return
    value from a successful call to exec. Note that children
    are not inherited: if A spawns child B and B spawns child
    process C, then A cannot wait for C, even if B is dead.
    A call to wait(C) by process A must fail. Similarly, orphaned
    processes are not assigned to a new parent if their parent
    process exits before they do.

    b. The process that calls wait has already called wait on PID.
    That is, a process may wait for any given child at most once.

    Processes may spawn any number of children, wait for them in
    any order, and may even exit without having waited for some or
    all of their children. The design should consider all the ways
    in which waits can occur. All of a process’s resources, including
    its struct thread, must be freed whether its parent ever waits
    for it or not, and regardless of whether the child exits before
    or after its parent.

    Must ensure that Pintos does not terminate until the initial
    process exits. The supplied Pintos code tries to do this by
    calling process_wait() (in‘userprog/process.c’) from main()
    (in ‘threads/init.c’). Implement process_wait() according to the
    comment at the top of the function and then implement the wait
    system call in terms of process_wait(). */
static int wait(tid_t tid) { return process_wait(tid); }

/* Creates a new file called FILE initially INITIAL_SIZE bytes in size.
    Returns true if successful, false otherwise. Creating a new file
    does not open it: opening the new file is a separate operation which
    would require an open system call. */
static bool create(const char *file, unsigned initial_size) {
  if (!check_str(file, 14))
    return false;

  acquire_file_lock();
  bool success = filesys_create(file, initial_size, false);
  release_file_lock();
  return success;
}

/* Deletes the file called FILE. Returns true if successful, false
    otherwise. A file may be removed regardless of whether it is open
    or closed, and removing an open file does not close it. */
static bool remove(const char *file) {
  if (!check_str(file, 14))
    return false;

  acquire_file_lock();
  bool success = filesys_remove(file);
  release_file_lock();
  return success;
}

/* Opens the file called FILE. Returns a nonnegative integer handle
    called a “file descriptor” (fd), or -1 if the file could not be
    opened.

    File descriptors numbered 0 and 1 are reserved for the console:
    fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is
    standard output. The open system call will never return either of
    these file descriptors, which are valid as system call arguments
    only.

    Each process has an independent set of file descriptors. File
    descriptors are not inherited by child processes.

    When a single file is opened more than once, whether by a single
    process or different processes, each open returns a new file
    descriptor. Different file descriptors for a single file are closed
    independently in separate calls to close and they do not share a
    file position. */
static int open(const char *file) {
  if (!check_str(file, 129))
    return -1;

  acquire_file_lock();
  struct file *file_open = filesys_open(file);
  if (file_open == NULL) {
    release_file_lock();
    return -1;
  }

  struct thread_file *thread_file = malloc(sizeof(struct thread_file));
  if (thread_file == NULL) {
    file_close(file_open);
    release_file_lock();
    return -1;
  }

  struct inode *inode = file_get_inode(file_open);
  if (inode != NULL && inode_is_directory(inode))
    thread_file->dir = dir_open(inode);
  else
    thread_file->dir = NULL;

  struct thread *cur = thread_current();
  thread_file->fd = cur->fd++;
  thread_file->file = file_open;

  list_push_back(&cur->files, &thread_file->elem);

  release_file_lock();
  return thread_file->fd;
}

/* Returns the size, in bytes, of the file open as FD. */
static int filesize(int fd) {
  struct thread_file *thread_file = find_file(fd);
  if (thread_file == NULL)
    return -1;
  if (isdir(fd))
    return -1;

  acquire_file_lock();
  int size = file_length(thread_file->file);
  release_file_lock();

  return size;
}

/* Reads SIZE bytes from the file open as FD into buffer. Returns
    the number of bytes actually read (0 at end of file), or -1 if
    the file could not be read (due to a condition other than end
    of file).

    Fd 0 reads from the keyboard using input_getc(). */
static int read(int fd, void *buffer, unsigned size) {
  check_write(buffer, size);

  if (fd == STDIN) { // fd == 0
    unsigned i;
    for (i = 0; i < size; i++)
      *(uint8_t *)(buffer + i) = input_getc();

    return size;
  } else {
    struct thread_file *thread_file = find_file(fd);
    if (thread_file == NULL)
      return -1;
    if (isdir(fd))
      return -1;

    acquire_file_lock();
    int bytes_read = file_read(thread_file->file, buffer, size);
    release_file_lock();

    return bytes_read;
  }
}

/* Changes the next byte to be read or written in open file FD to POSITION,
    expressed in bytes from the beginning of the file. (Thus, a position of
    0 is the file’s start.)

    A seek past the current end of a file is not an error. A later read
    obtains 0 bytes,indicating end of file. A later write extends the file,
    filling any unwritten gap with zeros. (However, in  Pintos files have a
    fixed length until project 4 is complete, so writes past end of file
    will return an error.) These semantics are implemented in the file
    system and do not require any special effort in system call implementation.
 */
static void seek(int fd, unsigned position) {
  struct thread_file *thread_file = find_file(fd);
  if (thread_file == NULL)
    return;
  if (isdir(fd))
    return;

  acquire_file_lock();
  file_seek(thread_file->file, position);
  release_file_lock();
}

/* Returns the position of the next byte to be read or written in open
    file FD, expressed in bytes from the beginning of the file. */
static unsigned tell(int fd) {
  struct thread_file *thread_file = find_file(fd);
  if (thread_file == NULL)
    return -1;
  if (isdir(fd))
    return -1;

  acquire_file_lock();
  unsigned position = file_tell(thread_file->file);
  release_file_lock();

  return position;
}

/* Closes file descriptor FD. Exiting or terminating a process implicitly
    closes all its open file descriptors, as if by calling this function
    for each one. */
static void close(int fd) {
  struct thread_file *thread_file = find_file(fd);
  if (thread_file == NULL)
    return;

  acquire_file_lock();
  file_close(thread_file->file);
  if (thread_file->dir) {
    free(thread_file->dir);
    thread_file->dir = NULL;
  }
  list_remove(&thread_file->elem);
  free(thread_file);
  release_file_lock();
}

#ifdef VM

/* Check if the address range is valid. */
static bool check_overlaps(void *addr, int size) {
  if (addr == NULL)
    return false;

  if (size < 0)
    return false;

  struct thread *cur = thread_current();
  for (int i = size; i >= 0; i -= PGSIZE) {
    if (find_spte(addr) || pagedir_get_page(cur->pagedir, addr))
      return false;
    addr += PGSIZE;
  }
  return true;
}

/* Make a new mmap entry */
static struct mmap_file *new_mmap_entry(void *addr, struct file *file) {
  /* Allocate space for new mmap entry */
  struct mmap_file *entry =
      (struct mmap_file *)malloc(sizeof(struct mmap_file));
  if (!entry)
    return NULL;

  /* Initialize the entry if malloc success */
  entry->mapid = thread_current()->mapid_cnt++;
  entry->base = addr;
  entry->file = file;
  return entry;
}

/* Map the file to the address space. */
static mapid_t mmap(int fd, void *addr) {
  /* Validate address. */
  if (addr == NULL || pg_ofs(addr) != 0)
    return -1;

  /* Validate fd. */
  if (fd == STDIN || fd == STDOUT)
    return -1;

  struct thread_file *file_desc = find_file(fd);
  if (file_desc == NULL)
    return -1;
  if (isdir(fd))
    return -1;

  mapid_t mapping = -1;
  struct file *file = NULL;
  bool spte_fail_midway = false;

  acquire_file_lock();

  /* Reopen file */
  file = file_reopen(file_desc->file);
  if (file == NULL) {
    release_file_lock();
    return -1;
  }

  /* Validate file size. */
  off_t file_size = file_length(file);
  if (file_size <= 0) {
    release_file_lock();
    return -1;
  }

  /* Check if the address range is valid. */
  if (!check_overlaps(addr, file_size)) {
    release_file_lock();
    return -1;
  }

  uint32_t real_bytes = file_size;
  uint32_t zero_bytes = (PGSIZE - real_bytes % PGSIZE) % PGSIZE;
  int page_count = (real_bytes + zero_bytes) / PGSIZE;
  if (page_count == 0 && real_bytes != 0)
    page_count = 1;

  struct mmap_file *mmap_entry = new_mmap_entry(addr, file);

  if (mmap_entry == NULL) {
    file_close(file);
    release_file_lock();
    return -1;
  }

  for (int i = 0; i < page_count; ++i) {
    void *cur_uaddr = addr + i * PGSIZE;
    off_t cur_ofs = i * PGSIZE;
    uint32_t cur_read_bytes = 0;
    if ((uint32_t)cur_ofs < real_bytes) {
      cur_read_bytes =
          (real_bytes - cur_ofs < PGSIZE) ? real_bytes - cur_ofs : PGSIZE;
    }
    uint32_t cur_zero_bytes = PGSIZE - cur_read_bytes;
    if (!lazy_load(file, cur_ofs, cur_uaddr, cur_read_bytes, cur_zero_bytes,
                   true)) {
      spte_fail_midway = true;
      break;
    }
  }
  if (spte_fail_midway) {
    file_close(file);
    free(mmap_entry);
    mmap_entry = NULL;
  } else {
    list_push_back(&thread_current()->mmap_list, &mmap_entry->elem);
    mapping = mmap_entry->mapid;
  }
  release_file_lock();
  return mapping;
}

/* Unmap the file from the address space. */
void munmap(mapid_t mapping_to_unmap) {
  struct thread *cur_thread = thread_current();
  struct list_elem *e;
  struct mmap_file *found_mmap_entry = NULL;

  for (e = list_begin(&cur_thread->mmap_list);
       e != list_end(&cur_thread->mmap_list); e = list_next(e)) {
    struct mmap_file *current_entry = list_entry(e, struct mmap_file, elem);
    if (current_entry->mapid == mapping_to_unmap) {
      found_mmap_entry = current_entry;
      list_remove(&current_entry->elem);
      break;
    }
  }

  if (found_mmap_entry == NULL) {
    return;
  }

  void *current_page_addr = found_mmap_entry->base;
  struct file *file_to_process = found_mmap_entry->file;

  off_t original_file_size = file_length(file_to_process);
  if (original_file_size < 0) {
    original_file_size = 0;
  }

  uint32_t real_bytes_in_mapping = original_file_size;
  uint32_t zero_bytes_padding =
      (PGSIZE - real_bytes_in_mapping % PGSIZE) % PGSIZE;
  int total_pages_in_mapping =
      (real_bytes_in_mapping + zero_bytes_padding) / PGSIZE;
  if (total_pages_in_mapping == 0 && real_bytes_in_mapping > 0) {
    total_pages_in_mapping = 1;
  }

  for (int i = 0; i < total_pages_in_mapping; ++i) {
    struct sup_page_table_entry *spte = find_spte(current_page_addr);

    if (spte) {
      if (pagedir_is_dirty(cur_thread->pagedir, spte->uaddr)) {
        acquire_file_lock();
        file_seek(spte->file, spte->offset);
        file_write(spte->file, current_page_addr, spte->read_bytes);
        release_file_lock();
      }

      if (pagedir_get_page(cur_thread->pagedir, spte->uaddr)) {
        frame_free(pagedir_get_page(cur_thread->pagedir, spte->uaddr));
        pagedir_clear_page(cur_thread->pagedir, spte->uaddr);
      }

      hash_delete(&cur_thread->sup_page_table, &spte->elem);
      free(spte);
    }
    current_page_addr = (void *)((char *)current_page_addr + PGSIZE);
  }

  acquire_file_lock();
  file_close(file_to_process);
  release_file_lock();

  free(found_mmap_entry);

  return;
}

#endif

/* Changes the current working directory of the process to DIR.
    Returns true if successful, false otherwise. */
static bool chdir(const char *dir) {
  if (!check_str(dir, NAME_MAX + 1))
    return false;

  acquire_file_lock();
  bool success = filesys_chdir(dir);
  release_file_lock();
  return success;
}

/* Creates a directory named DIR. Returns true if successful, false
    otherwise. A directory may be created regardless of whether it is
    open or closed, and creating an open directory does not close it.
    The directory is created with the initial size of 0 bytes. */
static bool mkdir(const char *dir) {
  if (!check_str(dir, NAME_MAX + 1))
    return false;

  acquire_file_lock();
  bool success = filesys_create(dir, 0, true);
  release_file_lock();
  return success;
}

/* Reads a directory entry from the directory open as FD.
    If successful, stores the name of the next entry in NAME
    and returns true. If there are no more entries in the
    directory, returns false. */
static bool readdir(int fd, char *name) {
  if (!check_str(name, NAME_MAX + 1))
    return false;

  struct thread_file *thread_file = find_file(fd);
  bool success = false;

  acquire_file_lock();
  if (thread_file != NULL && thread_file->dir != NULL)
    success = dir_readdir(thread_file->dir, name);
  release_file_lock();

  return success;
}

/* Returns true if the file descriptor FD is a directory.
    Returns false otherwise. */
static bool isdir(int fd) {
  struct thread_file *thread_file = find_file(fd);
  bool result = false;

  acquire_file_lock();
  struct inode *inode = file_get_inode(thread_file->file);
  if (inode != NULL)
    result = inode_is_directory(inode);
  release_file_lock();

  return result;
}

/* Returns the inode number of the file open as FD.
    Returns -1 if the file descriptor is invalid or
    if the file does not have an inode. */
static int inumber(int fd) {
  struct thread_file *thread_file = find_file(fd);
  int inode_number = -1;

  acquire_file_lock();
  struct inode *inode = file_get_inode(thread_file->file);
  if (inode != NULL)
    inode_number = inode_get_inumber(inode);
  release_file_lock();

  return inode_number;
}
