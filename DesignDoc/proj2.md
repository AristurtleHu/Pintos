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

The fd is unique just within a single process.
The file descriptor (fd) is allocated by the system call `open`, using the smallest unused integer in the current process. The kernel creates a `struct thread_file` object, binds the `fd` to the underlying `struct file`, and adds this object to the process's file list. During file operations, the process locates the corresponding `thread_file` via the `fd` and accesses the actual file object through its file member to perform read/write operations.



### Algorithms

> **B3:** Describe your code for reading and writing user data from the kernel.
 
Firstly, we check that a user pointer point is below `PHYS_BASE`.
An invalid user pointer will cause a "page fault" that handled in "userprog/exception.c" and we terminates the process with `exit_code = -1`.

Next, we read and write.

Read:
The function first validates the buffer using `check_write(buffer, size)`. For `STDIN` input, it reads characters individually from the console via `input_getc()`. 
For other file descriptors, it locates the target file through `find_file()` and executes `file_read()`. (with lock acquire and release)

Write:
In the case of `STDOUT`, the output is directly printed to the console using `putbuf()`. 
For other file descriptors, the system locates the appropriate file through `find_file()` and performs the write operation via `file_write()`. (with lock acquire and release)




> **B4:** Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel. What is the least and the greatest possible number of inspections of the page table (e.g., calls to `pagedir_get_page()`) that might result? What about for a system call that only copies 2 bytes of data? Is there room for improvement in these numbers, and how much?
 
If the user buffer is 4KB-aligned, only one check is required. If it spans two pages, two checks are necessary.

When two bytes reside within a single page, only one validation is required. If they cross page boundaries, two separate checks must be performed.

Pre-validate all pages covering the user buffer before performing the copy operation. Per-byte invocations of `pagedir_get_page()` (such as within copy loops) cause repeated page validation. A 4KB-aligned data copy would perform 4096 unnecessary validations where a single check would suffice. 


> **B5:** Briefly describe your implementation of the "wait" system call and how it interacts with process termination.
 
Call `process_wait()`.

We define a new `struct child` to represent child's exit status. And a list of `child` is added into parent's thread struct. 

* Get the list of child processes (`children`) of the current process (the parent).
* Iterate through this list, using the `list_entry` to get the corresponding `struct child` for each list element.
* Compare the `tid` of each child process with the input `child_tid`.
* If a matching child process is found, first check its `is_waited` flag. If the flag is `true`, it means this child process has already been waited upon by another `wait` call; return `-1` immediately.
* If not already waited upon, set the child process's `is_waited` flag to `true`, indicating that the parent process is now starting to wait for it.
* When `sema_down` returns, it signifies that the child process has terminated. At this point, read the exit code (`child->exit_code`) set by the child from the `child` structure. Then, call `list_remove(e)` to remove the entry for this child process from the parent's `children` list, completing the resource cleanup.
* If the entire `children` list is traversed without finding a child process with a matching `tid`, the function returns `-1`, indicating that the specified child process does not exist or is not a child of the current process.


> **B6:** Accessing user program memory at a user-specified address may fail due to a bad pointer value, requiring termination of the process. Describe your strategy for managing error-handling without obscuring core functionality and ensuring that all allocated resources (locks, buffers, etc.) are freed. Give an example.

1. Memory Access Safety

* Pre-validate user addresses with `is_user_vaddr(uaddr)` before access.
* Perform per-byte dynamic checks during actual operations. Using `get_user()` and `put_user()`
* If the pointer we check about is a bad pointer we call `exit(-1)`.
 
```C
// helper function for checking ptr 
check_address() /* Check address validity */;
check_str()     /* Check if str is a valid string in user space. */;
check_write()   /* Check if str is able to write */
```

2. Lock Management

* Use `acquire_file_lock()` and `release_file_lock()` to ensure:
  * Locks are acquired before critical sections.
  * Guaranteed release on both success and error paths.

3. Resource Cleanup

* Centralized resource release via `process_exit()`.
* Handles all allocations.
* Serves as single cleanup point on termination.

For example, in syscall `read()`, we first check if the address of the buffer is vaild. if not, then we call `exit(-1)` to exit. Then the `thread_exit()` will do `sema_up` for ever child, free the space of them, then close all the files of the thread and free.


### Synchronization

> **B7:** The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading. How does your code ensure this? How is the load success/failure status passed back to the thread that calls "exec"?

1.  First, in `process_execute()`, we initialize `tid` with the return value of `thread_create()` (which starts `start_process`) and set up a semaphore `sema` initialized to 0 for synchronization. We immediately check if `tid == TID_ERROR` (thread creation failure) and return `-1` if it is.

2.  Then, in `start_process()`, the new thread attempts to `load()` the executable. After the attempt, it records the success/failure status to its parent thread's `load_state` and then performs `sema_up(&sema)` to signal `process_execute`. If loading failed, it sets its own exit code to `-1` and calls `thread_exit()`.

3.  Finally, back in `process_execute()` (after the `tid != TID_ERROR` check), we call `sema_down(&sema)` to wait for `start_process` to finish loading. Once `sema_down` returns, we check the recorded load success/failure status. If loading failed, `process_execute` returns `-1`; otherwise (if loading succeeded), it returns the successfully obtained `tid`.
  
The success/failure status is kept in `load_state` owned by each thread.


> **B8:** Consider a parent process P with child process C. How do you ensure proper synchronization and avoid race conditions when:
> - P calls `wait(C)` before C exits?
> - P calls `wait(C)` after C exits?
> - P terminates without waiting, before C exits?
> - P terminates after C exits?
> - Are there any special cases?

**P calls `wait(C)` before C exits?**

* P calls `process_wait(C)`, search in the list children to find C, and calls `sema_down()` on C's associated semaphore, causing P to block.
* Later, C calls `process_exit()`. In this function, C frees all its allocated resources. Before terminating, C calls `sema_up()` on the semaphore.
* The `sema_up()` call unblocks P. P returns from `sema_down()`, completes the `process_wait()` call (retrieving status if applicable), and continues execution.

**P calls `wait(C)` after C exits?**

* C calls `process_exit()`. Its resource has been already freed. It calls `sema_up()` before terminating.
* Later, P calls `process_wait(C)`. It identifies C and calls `sema_down()` on the semaphore.
* Since `sema_up()` was already called by C, `sema_down()` returns immediately without blocking P.

**P terminates without waiting, before C exits?**
 
* P calls `process_exit()`. Its resources will be freed. Exit code is renewed for C and called `sema_up`
```C
thread_current()->thread_child->exit_code = thread_current()->exit_code;
 sema_up(&thread_current()->thread_child->sema);
``` 
* C is still alive and continues running.
* C proceeds to free its own resources within its `process_exit()` and terminates. Resources from both P and C are eventually freed.

**P terminates without waiting, after C exits?**

* C calls `process_exit()`. C is "dead".
* Later, P calls `process_exit()`. It isn't waited for C.
* P frees its resources when P exits.
* Since C already exited and freed its resources, P's exit simply cleans up P's own state, including any remaining tracking info for the already-terminated C.

**Are there any special cases?**

C calls wait and then P calls wait.

* Using `bool is_waited` to identify if a child is waiting. If true, just return -1 for the call of wait. 




### Rationale

> **B9:** Why did you choose to implement access to user memory from the kernel in the way that you did?

 Our implementation ensures kernel stability by validating user addresses prior to any access. 
 
 This pre-validation step is essential to guarantee that potentially invalid or malicious pointers from user space do not compromise or crash the kernel.

> **B10:** What advantages or disadvantages can you see to your design for file descriptors?

Advantages:

1. Space Usage is minimized
2. Kernel is aware of all the open files. Easy to manage opened files in a unique thread or process.

Disadvantages:

1. Consumes kernel space, user program may open lots of files to crash the kernel.
2. No ability to know which thread owns the file.
3. Accessing a fd is `O(n)`, where n is the number of fd for the current thread.



> **B11:** The default `tid_t` to `pid_t` mapping is the identity mapping. If you changed it, what advantages does your approach offer?

We did not modify the default identity mapping between `tid_t` and `pid_t`. This approach is simple and efficient for our implementation, as Pintos currently doesn't support multiple threads per process. 

If we were to implement multithreading, we might consider a mapping like `pid << 10 + thread_number` which would allow up to 1024 threads per process while making it easy to extract the process ID from any thread ID.



---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

None.
