# CS 130: Project 2 - User Programs Design Document

## Group Information

Please provide the details for your group below, including your group's name and the names and email addresses of all members.

- **Group Name**: *boeing*
- **Member 1**: Renyi Yang `<yangry2023@shanghaitech.edu.cn>`
- **Member 2**: Jiaxing Wu `<wujx2023@shanghaitech.edu.cn>`



---

## Preliminaries

> If you have any preliminary comments on your submission, notes for the TAs, or extra credit, please give them here.

None.



---

## Argument Passing

### Data Structures

> **A1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

We didn't declare any new structs or change struct members or use global or static variables for argument passing.

We just implement the passing during `start_process()`


### Algorithms

> **A2:** Briefly describe how you implemented argument parsing. How do you arrange for the elements of `argv[]` to be in the right order? How do you avoid overflowing the stack page?

Our implementation follows these steps:
1. We parse the command line into tokens using `strtok_r()`. 
2. We count the number of arguments to allocate appropriate space and use the argv array to store the address
3. We create a copy of each argument string on the stack, working downward from the stack pointer.
(The order is not important so we didn't use a reverse one for storing the string)
4. We align the stack to 4 bytes, add a `NULL` argument address at the beginning and storing pointers to each argument (This need to be in reverse)
5. We push address of `argv[0]`, `argc`, and a return address onto the stack.

The correct order of reversing arguments in stack is not required, just like the document said. However the address of them must be in reverse order. Therefore, we use a argv[] to store arguments' address when pushing into stack. Then downward `4 * (argc + 1)` and use `memcpy()` will push the address as required.

For stack overflow prevention, we don't pre-calculate the required space. Instead, we let the page fault handler catch any stack overflow conditions that might occur during argument passing. When a page fault happens with an invalid address, we terminate the process with `exit_code = -1`. This approach simplifies our implementation while still correctly handling excessive arguments that would overflow the stack page.


### Rationale

> **A3:** Why does Pintos implement `strtok_r()` but not `strtok()`?

Pintos implements `strtok_r()` instead of `strtok()` because `strtok_r()` is reentrant and thread-safe. `strtok()` uses inner variables to maintain state between calls. In contrast, the `save_ptr` in `strtok_r()` is provided by the caller, allowing multiple parsing operations to happen concurrently without interference. This makes the situation safe if there were more than one thread call `strtok()_r`, as each thread have pointer (`save_ptr`) which is independent from the caller.


> **A4:** In Pintos, the kernel separates commands into an executable name and arguments, while Unix-like systems have the shell perform this separation. Identify at least two advantages of the Unix approach.

Advantages of the Unix approach:
1. It reduces kernel complexity by delegating parsing logic to user-level programs.
2. It enables greater flexibility in command interpretation, allowing for shell-specific features like wildcards and variable expansion.
3. It improves security by not requiring the kernel to directly handle potentially malicious user input during parsing.
4. It allows for different shells with different parsing behaviors without modifying the kernel.



---

## System Calls

### Data Structures

> **B1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.
 
```C
/* Lock used by file operations. 
  Ensure that only one process at a time
  is read/write the file system */
static struct lock file_lock;

/* File descriptor 
  STDIN = 0
  STDOUT = 1 */
enum fd { STDIN, STDOUT, STDERR };

/* Load state: success or not */
enum load_state {
  INIT,    /* Loading. */
  SUCCESS, /* Success. */
  FAIL     /* Failed. (exit -1 finally) */
};

struct thread {
#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint32_t *pagedir; /* Page directory. */
  int exit_code;     /* Process exit code. */

  struct list children;       /* List of child processes. */
  struct thread *parent;      /* Parent process. */
  struct child *thread_child; /* Child thread. */
  struct semaphore sema;      /* Semaphore for process exit. */
  enum load_state load_state; /* State if loading success. */

  struct file *exec_file; /* Exec file held by the thread. */
  struct list files;      /* List of open files. */
  int fd;                 /* File descriptor. */
#endif
};

struct child {
  tid_t tid;             /* Child thread id */
  int exit_code;         /* Exit code of the child */
  struct list_elem elem; /* List element */
  struct semaphore sema; /* Semaphore for process exit */
  bool is_waited;        /* True if the parent has waited for the child */
};

struct thread_file {
  int fd;                /* File descriptor */
  struct file *file;     /* File pointer */
  struct list_elem elem; /* List element */
};
```


> **B2:** Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?

The file descriptor (fd) is allocated by the system call `open`, using the smallest unused integer in the current process. The kernel creates a `struct thread_file` object, binds the `fd` to the underlying `struct file`, and adds this object to the process's file list. During file operations, the process locates the corresponding `thread_file` via the `fd` and accesses the actual file object through its file member to perform read/write operations.
The fd is unique just within a single process.



### Algorithms

> **B3:** Describe your code for reading and writing user data from the kernel.
Read:
The function first validates the buffer using check_write(buffer, size). For STDIN input, it reads characters individually from the console via input_getc(). 
For other file descriptors, it locates the target file through find_file and executes file_read.
Write:
In the case of STDOUT, the output is directly printed to the console using putbuf. For other file descriptors, the system locates the appropriate file and performs the write operation via file_write.




> **B4:** Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel. What is the least and the greatest possible number of inspections of the page table (e.g., calls to `pagedir_get_page()`) that might result? What about for a system call that only copies 2 bytes of data? Is there room for improvement in these numbers, and how much?
If the user buffer is 4KB-aligned, only one check is required. If it spans two pages, two checks are necessary.When two bytes reside within a single page, only one validation is required. If they cross page boundaries, two separate checks must be performed.
Per-byte invocations of `pagedir_get_page` (such as within copy loops) cause repeated page validation. A 4KB-aligned data copy would perform 4096 unnecessary validations where a single check would suffice,Pre-validate all pages covering the user buffer before performing the copy operation.



> **B5:** Briefly describe your implementation of the "wait" system call and how it interacts with process termination.
* Locate the child process using the provided `child_tid`.
* If the child is already marked as `is_waited`, return `-1`.
* Otherwise, set `is_waited = true`.
termination:
* Wake up the parent process.
* Retrieve the exit code via `child->exit_code`.
* Remove the child entry from the linked list.
* Return the exit code to the parent.
* Pass `exit_code` to the parent thread.
* Release all resources back to the parent thread.


> **B6:** Accessing user program memory at a user-specified address may fail due to a bad pointer value, requiring termination of the process. Describe your strategy for managing error-handling without obscuring core functionality and ensuring that all allocated resources (locks, buffers, etc.) are freed. Give an example.
## Memory Access Safety

* Pre-validate user addresses with `is_user_vaddr(uaddr)` before access.
* Perform per-byte dynamic checks during actual operations.

## Lock Management

* Use `acquire_file_lock()` and `release_file_lock()` to ensure:
    * Locks are acquired before critical sections.
    * Guaranteed release on both success and error paths.

## Resource Cleanup

* Centralized resource release via `process_exit()`.
* Handles all allocations.
* Serves as single cleanup point on termination.


### Synchronization

> **B7:** The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading. How does your code ensure this? How is the load success/failure status passed back to the thread that calls "exec"?

*Your answer here.*



> **B8:** Consider a parent process P with child process C. How do you ensure proper synchronization and avoid race conditions when:
> - P calls `wait(C)` before C exits?
> - P calls `wait(C)` after C exits?
> - P terminates without waiting, before C exits?
> - P terminates after C exits?
> - Are there any special cases?

*Your answer here.*



### Rationale

> **B9:** Why did you choose to implement access to user memory from the kernel in the way that you did?

*Your answer here.*



> **B10:** What advantages or disadvantages can you see to your design for file descriptors?

*Your answer here.*



> **B11:** The default `tid_t` to `pid_t` mapping is the identity mapping. If you changed it, what advantages does your approach offer?

We did not modify the default identity mapping between `tid_t` and `pid_t`. This approach is simple and efficient for our implementation, as Pintos currently doesn't support multiple threads per process. 

If we were to implement multithreading, we might consider a mapping like `pid << 10 + thread_number` which would allow up to 1024 threads per process while making it easy to extract the process ID from any thread ID.



---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

None.
