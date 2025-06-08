# CS 130: Project 4 - File Systems Design Document

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

## Indexed and Extensible Files

### Data Structures

> **A1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



> **A2:** What is the maximum size of a file supported by your inode structure? Show your work.

*Your answer here.*

The maximum file size supported by this inode structure is 8,517,120 bytes, which is approximately 8.12 MB.

This limitation is derived from the design of the struct inode_disk in inode.c, which defines how data blocks for a file are indexed. The structure uses a combination of direct, singly indirect, and doubly indirect pointers to locate all the data blocks that constitute a file. The calculation assumes a standard block size (BLOCK_SECTOR_SIZE) of 512 bytes

### Synchronization

> **A3:** Explain how your code avoids a race if two processes attempt to extend a file at the same time.

My code prevents race conditions during simultaneous file extensions by employing a single, global file system lock, `file_lock`, to ensure that all file system operations are serialized. When a process issues a `write` system call, the handler in `syscall.c` first acquires this lock via `acquire_file_lock()` before proceeding with the write operation. This ensures that the critical section for file extension within the `inode_write_at` function—which includes allocating new blocks with `inode_allocate_sector` and updating the inode's length metadata—is executed as an atomic operation. Any other process attempting a file system operation will block until the first process completes its write and calls `release_file_lock()`, thereby guaranteeing that only one process can modify the file system's state at any given time and preventing data corruption.


> **A4:** Suppose processes A and B both have file F open, both positioned at end-of-file. If A reads and B writes F at the same time, A may read all, part, or none of what B writes. However, A may not read data other than what B writes (e.g., if B writes nonzero data, A is not allowed to see all zeros). Explain how your code avoids this race.

My code avoids this race condition by using a single, global `file_lock` to serialize all file system operations. Both the `read` and `write` system call handlers in `syscall.c` are required to obtain this same lock by calling `acquire_file_lock()` before performing any file I/O and must release it afterward. This design makes the entire `read` and `write` operations atomic with respect to each other, meaning process A's read cannot be interleaved with process B's write. Consequently, process A is prevented from observing an inconsistent state—such as newly allocated but not-yet-written zero-filled blocks—because process B holds the lock for the entire duration of its write, from allocating blocks to writing the actual data. Process A will therefore only ever read the file's contents either entirely before B's write begins or entirely after it has successfully completed, guaranteeing data consistency.


> **A5:** Explain how your synchronization design provides "fairness." File access is "fair" if readers cannot indefinitely block writers or vice versa—meaning that many readers do not prevent a writer, and many writers do not prevent a reader.

The synchronization design provides fairness because the global `file_lock` serializes all file system requests into a single First-In, First-Out (FIFO) queue, preventing starvation for both readers and writers. Whenever any process attempts a file operation, such as `read` or `write`, it must call `acquire_file_lock()`; if the lock is held, the calling thread is added to the lock's wait list and sleeps. Crucially, because the underlying lock implementation serves waiting threads in FIFO order, the lock is granted to the process that has been waiting the longest, regardless of whether it is a reader or a writer. This strictly ordered, turn-based access ensures that a stream of readers cannot indefinitely block a waiting writer, nor can a stream of writers starve a reader, as each will be served in the order it arrived.

### Rationale

> **A6:** Is your inode structure a multilevel index? If so, why did you choose this particular combination of direct, indirect, and doubly indirect blocks? If not, why did you choose an alternative inode structure, and what advantages and disadvantages does your structure have compared to a multilevel index?

Yes, my inode structure is a multilevel index, composed of direct pointers, a single indirect block pointer, and a doubly indirect block pointer as defined in the `inode_disk` struct. This particular combination was chosen to balance performance for small, common files with scalability for larger ones; the 123 direct block pointers (`DIRECT_BLOCKS_COUNT`) provide fast, single-I/O access for small files, while the indirect and doubly indirect blocks allow the file to grow significantly without changing the inode's fixed size. The number of direct pointers is precisely 123 because this is the maximum number of pointers that can fit into a single disk sector after accounting for the inode's other metadata fields and indirect pointers, a constraint verified within the `inode_create` function. A triple-indirect block was omitted as a design trade-off to maintain simplicity, as the resulting maximum file size of approximately 8.12MB was considered sufficient for the system's requirements.


---

## Subdirectories

### Data Structures

> **B1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



### Algorithms

> **B2:** Describe your code for traversing a user-specified path. How do traversals of absolute and relative paths differ?

My code traverses user-specified paths using the `dir_open_path` function, which tokenizes the input string by the `/` delimiter and iteratively descends through the directory structure. The traversal of absolute and relative paths differs primarily in their starting point: an absolute path (one beginning with `/`) always starts from the file system's root directory, while a relative path starts from the current thread's working directory (`cwd`). For each token in the path, the code performs a `dir_lookup` within the current directory to find the inode for the next component. This lookup process correctly handles special names, returning a handle to the current directory for `.` and the parent directory for `..`. The traversal continues until all tokens are processed, returning a handle to the final directory, or it fails and returns `NULL` if any component along the path is not found.


### Synchronization

> **B4:** How do you prevent races on directory entries? For example, only one of two simultaneous attempts to remove a single file should succeed, as should only one of two simultaneous attempts to create a file with the same name.

Races on directory entries are prevented by the same global `file_lock` that serializes all file system operations, ensuring that any sequence of checks and modifications to a directory is atomic. Both the `create` and `remove` system calls acquire this lock before executing and only release it upon completion. The core logic for these operations involves a "check-then-act" pattern: `dir_add` first looks up a name to ensure it does not already exist before adding a new entry, and `dir_remove` looks up a name to confirm its existence before deleting it. Because the global lock protects this entire sequence, it is impossible for two processes to interleave their checks and actions. For example, if two processes attempt to create a file with the same name, the first to acquire the lock will succeed, and the second will fail its check because the name now exists; similarly, only the first of two simultaneous attempts to remove a file will find the entry and succeed.


> **B5:** Does your implementation allow a directory to be removed if it is open by a process or if it is in use as a process's current working directory?  
> If so, what happens to that process's future file system operations? If not, how do you prevent it?

Yes, my implementation allows a directory to be removed while it is open by a process or is in use as a process's current working directory, because the `dir_remove` function only checks if a directory is empty, not if it is currently open. The removal does not immediately invalidate existing handles; instead, it employs a "soft delete" mechanism where `inode_remove` simply sets a `removed` flag on the directory's in-memory inode. A process with an existing open handle (including the current working directory) can continue to perform operations within that directory, as its inode pointer remains valid. However, once the last process using that directory closes its handle, the `inode_close` function will detect that the `removed` flag is set and will proceed to deallocate the inode and its data blocks, completing the deletion. Any new attempt to look up the directory by its path will fail immediately, as its entry has already been erased from the parent directory.



### Rationale

> **B6:** Explain why you chose to represent the current directory of a process the way you did.

I chose to represent the current working directory as a `struct dir *cwd` pointer within each process's `struct thread` because it is a highly efficient design that directly integrates with the existing file system logic. By maintaining a pointer to an already open directory structure, the system avoids the overhead of repeatedly traversing the path from the root to find the current directory for every relative path operation; instead, lookups can begin immediately from the cached `cwd`. Placing this pointer within the `struct thread` ensures that the current working directory is an independent part of each process's context, allowing one process to change its directory without affecting any others. This approach also leverages the file system's existing resource management, as the `cwd` handle participates in the inode's reference counting, ensuring that a directory's resources are not deallocated while it is still in use as a process's working directory.



---

## Buffer Cache

### Data Structures

> **C1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



### Algorithms

> **C2:** Describe how your cache replacement algorithm chooses a cache block to evict.

My implementation uses the clock algorithm, a variation of the second-chance algorithm, to choose a cache block for eviction, as implemented in the `cache_evict` function. This algorithm maintains a 'clock' hand that iterates cyclically through the cache entries. When the clock hand points to a cache block, it inspects the block's `access` bit; if this bit is set to true, indicating recent use, the algorithm gives the block a second chance by setting the bit to false and advancing the hand. If the `access` bit is false, that block is selected as the victim for eviction. Before the victim block is marked as invalid, the algorithm checks if it is 'dirty' and, if so, writes its contents back to disk using the `write_back` function to ensure data integrity.



> **C3:** Describe your implementation of write-behind.

My implementation of write-behind is achieved by modifying blocks only in the buffer cache and deferring the actual disk write. When `cache_write` is called, it copies the new data into the corresponding cache block and sets that block's `dirty` flag to true, but does not immediately write the data to disk. The actual disk write is postponed until one of two events occurs: either the dirty block is selected for eviction by the `cache_evict` algorithm, or the file system is shut down. In both cases, the `write_back` function is called to flush the modified data from the cache to the disk, ensuring data persistence. This strategy reduces disk I/O by consolidating multiple writes to the same block into a single operation and allows write operations to return quickly to the caller.



> **C4:** Describe your implementation of read-ahead.

Based on an analysis of the provided source code, a read-ahead caching strategy has not been implemented. The caching system operates strictly on-demand; when a block is requested via the `cache_read` function, only that specific block is fetched from the disk into the cache if it is not already present. The code contains no logic to preemptively fetch subsequent blocks, such as sector N+1, in anticipation of future sequential reads.


### Synchronization

> **C5:** When one process is actively reading or writing data in a buffer cache block, how are other processes prevented from evicting that block?

Other processes are prevented from evicting a cache block that is in active use because all cache operations are protected by a single, global `cache_lock`. Any process that needs to read or write data must first acquire this lock at the beginning of the `cache_read` or `cache_write` functions and only releases it after the entire operation, including copying data to or from the block's buffer, is complete. Because an eviction can only be triggered from within these same locked functions, it is impossible for one process to be considering a block for eviction while another process is actively performing a data copy on it. This serialization ensures that finding, using, and potentially evicting a block is an atomic operation, guaranteeing that a block is never evicted while it is in the middle of being accessed.


> **C6:** During the eviction of a block from the cache, how are other processes prevented from attempting to access the block?

Other processes are prevented from accessing a block during its eviction because the entire cache system is protected by a single, global `cache_lock`, which serializes all cache-related functions. The process that triggers an eviction by calling `cache_read` or `cache_write` holds this lock for the entire duration of the operation, including the call to the `cache_evict` function. If another process attempts to access any block by calling `cache_read` or `cache_write` while the eviction is underway, it will immediately block upon trying to acquire the already-held `cache_lock`. This guarantees that the eviction process—which may include writing the block back to disk and marking it as invalid—completes as an atomic unit without any possibility of another process reading or modifying the block in its intermediate state.


### Rationale

> **C7:** Describe a file workload likely to benefit from buffer caching, and workloads likely to benefit from read-ahead and write-behind.

*Your answer here.*

A workload with high temporal locality, such as repeatedly accessing a database index or a file system's metadata blocks, would significantly benefit from the buffer cache, as subsequent reads of the same block are served directly from memory via `find_cache` without slow disk I/O. Workloads involving bursts of writes or frequent modifications to the same blocks, like transaction logging or repeatedly saving a document, benefit from the implemented write-behind policy; the `cache_write` function defers disk I/O by only marking blocks as 'dirty' in the cache, which improves application responsiveness and can consolidate multiple updates into a single disk write later. Finally, while not implemented in the provided code, a read-ahead strategy would most benefit workloads characterized by large, sequential file access, such as streaming media files or performing full-table database scans, by preemptively fetching subsequent blocks into the cache to minimize read latency.

---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

None.

