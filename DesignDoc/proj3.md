# CS 130: Project 3 - Virtual Memory Design Document

## Group Information

Please provide the details for your group below, including your group's name and the names and email addresses of all members.

- **Group Name**: *[Enter your group name here]*

- **Member 1**: Renyi Yang `<email@domain.example>`

- **Member 2**: Jiaxing Wu `<wujx2023@shanghaitech.edu.cn>`

    

---

## Preliminaries

> If you have any preliminary comments on your submission, notes for the TAs, or extra credit, please give them here.

*Your answer here.*



---

## Page Table Management

### Data Structures

> **A1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



### Algorithms

> **A2:** In a few paragraphs, describe your code for accessing the data stored in the SPT about a given page.

To access the metadata stored in my Supplemental Page Table (SPT) for a given user virtual page `uaddr`, I use the `find_spte(uaddr)` function. This function first aligns the `uaddr` to its page boundary using `pg_round_down()`. It then uses this page-aligned address as a key to search the current thread's `sup_page_table` (which is a hash table) via `hash_find()`. If a `struct hash_elem` is found, `hash_entry()` is used to retrieve the corresponding `struct sup_page_table_entry` pointer; otherwise, if no entry exists for that `uaddr`, `NULL` is returned. This `find_spte()` function is the central method used throughout my virtual memory system to look up page-specific information.

> **A3:** How does your code coordinate accessed and dirty bits between kernel and user virtual addresses that alias a single frame, or alternatively how do you avoid the issue?

My system coordinates accessed (A) and dirty (D) bits primarily by relying on the CPU to set these bits in the user process's hardware Page Table Entry (PTE) upon access to a user virtual address (`uaddr`). Kernel routines, such as `evict_frame` for page replacement or `munmap` for writing back data, then query the status of these bits directly from the user's PTE using functions like `pagedir_is_accessed(owner_pagedir, uaddr)` and `pagedir_is_dirty(owner_pagedir, uaddr)`. When the kernel writes to a frame via its kernel address `kaddr` (e.g., during `load_page`), the page is populated but is considered clean from the user's perspective until an actual user write to `uaddr` sets the D-bit. For specific kernel operations that logically "dirty" a page on behalf of the user (like `stack_grow` initializing a new, writable stack page), I explicitly call `pagedir_set_dirty(pagedir, uaddr, true)` on the user's PTE to maintain consistency, thereby largely avoiding A/D bit aliasing issues by focusing on the user virtual address's PTE state.

### Synchronization

> **A4:** When two user processes both need a new frame at the same time, how are races avoided?

In my implementation (primarily in `frame.c`), races are avoided when two user processes concurrently need a new frame by using a single global mutex called `frame_lock`. The `frame_alloc()` function, which is responsible for providing a physical frame, acquires `frame_lock` at its beginning and holds it until the entire allocation process, including any potential page eviction via `evict_frame()`, is complete. This lock protects all critical operations involving shared frame management data structures, such as checking the `palloc` system for free pages, selecting a victim frame using the clock algorithm within `evict_frame()`, modifying the global `frame_table` list (e.g., adding a new frame entry or removing an evicted one), and interacting with the swap system via `swap_out()`. By serializing these operations, `frame_lock` ensures that the frame allocation and eviction logic behaves as an atomic unit with respect to concurrent requests, thus preventing race conditions.

### Rationale

> **A5:** Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

For representing virtual-to-physical (or to backing store) mappings at the operating system level, I chose a **per-thread hash table to implement the Supplemental Page Table (SPT)**, where each entry is a `struct sup_page_table_entry` (SPTE). I selected a hash table primarily because it offers efficient average O(1) time complexity for looking up an SPTE using the page-aligned user virtual address (`uaddr`) as the key; this is critical for fast page fault handling, overlap checks, and managing potentially sparse user virtual address spaces. The `struct sup_page_table_entry` itself is designed to store all necessary metadata to manage a virtual page throughout its lifecycle, including its `type` (e.g., `PAGE_FILE`, `PAGE_ZERO`, `PAGE_STACK`), backing store information (like a `file` pointer, `offset` in the file, `read_bytes`, and `zero_bytes` for file-backed pages), its current status and location (e.g., kernel virtual address `kaddr` if in a frame, or a `swap_index` if swapped out), and its access permissions (`writable`).

---

## Paging To and From Disk

### Data Structures

> **B1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



### Algorithms

> **B2:** When a frame is required but none is free, some frame must be evicted. Describe your code for choosing a frame to evict.

My frame eviction strategy, implemented in the `evict_frame()` function within `frame.c`, uses a clock (or second-chance) algorithm. I maintain a global `frame_table` (a list of `frame_table_entry` structs) and a `clock_ptr` that circularly traverses this list. When eviction is necessary, my code iterates from the `clock_ptr`'s current position: if a frame is pinned (`fte->pinned`), it is skipped. For an unpinned frame, I check its accessed bit using `pagedir_is_accessed()`; if this bit is set, I clear it using `pagedir_set_accessed(..., false)` to give the page a second chance, and the clock hand advances. If the accessed bit is already clear, that frame is chosen as the victim. Its content is then written to a swap slot via `swap_out()`, its hardware page table entry is cleared using `pagedir_clear_page()`, its Supplemental Page Table Entry (SPTE) is updated (setting `kaddr = NULL` and storing the `swap_index`), the `frame_table_entry` struct is freed, and the kernel virtual address of the now-available physical frame is returned for reuse.

> **B3:** When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

When my process P obtains a physical frame previously used by process Q (this frame's kernel address `kaddr` having been returned by `evict_frame()` after evicting Q's page), the adjustments occur in two main stages. Firstly, during the eviction of Q's page by my `evict_frame()` function: Q's Supplemental Page Table Entry (SPTE_Q) was updated (its `kaddr` field was set to `NULL`, and its `swap_index` was recorded if written to swap), and crucially, Q's hardware Page Table Entry (PTE) for its original virtual address `uaddr_Q` was cleared using `pagedir_clear_page(Q's_pagedir, uaddr_Q)`, thereby invalidating Q's mapping to that physical frame. Secondly, when this physical frame is allocated to process P by my `frame_alloc()` function, a new `frame_table_entry` is created associating the `kaddr` with P's target SPTE (SPTE_P) and P as the owner. Then, the code path in P that called `frame_alloc` (typically P's page fault handler, like `load_page`, or a function like `stack_grow`) calls `install_page(uaddr_P, kaddr, permissions_for_P)`. This `install_page` call modifies process P's hardware page table (`P's_pagedir`) to establish a new, valid mapping from P's distinct virtual address `uaddr_P` to this reused physical frame.

> **B4:** Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the page that faulted.

*Your answer here.*



### Synchronization

> **B5:** Explain the basics of your VM synchronization design. In particular, explain how it prevents deadlock. (Refer to the textbook for an explanation of the necessary conditions for deadlock.)

My VM synchronization relies on several locks: a global `frame_lock` for the frame table and allocation/eviction decisions; per-SPTE `spte_lock` for individual supplemental page table entry state changes; a global `swap_lock` for the swap bitmap and device access; and a global `filesys_lock` for file system operations. Deadlock prevention is primarily attempted by establishing a conceptual lock ordering. However, my current `load_page` acquires an `spte_lock` *before* calling `frame_alloc` (which acquires `frame_lock`), while `evict_frame` (called under `frame_lock`) acquires a victim's `spte_lock`. This specific interaction (`spte_lock` then `frame_lock` in one path, and `frame_lock` then `spte_lock` in another) presents a classic circular wait potential and thus a deadlock risk. To properly prevent this, `load_page` should release its `spte_lock` before calling `frame_alloc` and then re-evaluate the SPTE's state after reacquiring the lock, or a strict global lock acquisition order must be enforced for all code paths (e.g., always `frame_lock` before `spte_lock`).

> **B6:** A page fault in process P can cause another process Q's frame to be evicted. How do you ensure that Q cannot access or modify the page during the eviction process? How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

When process P's page fault triggers the eviction of process Q's frame (`uaddr_Q`), my `evict_frame` function ensures Q cannot access or modify the page's old content in that frame by first acquiring the lock for Q's specific SPTE (`spte_Q->spte_lock`). While holding this lock, `evict_frame` calls `pagedir_clear_page(Q's_pagedir, uaddr_Q)` to remove Q's hardware page table mapping to the physical frame *before* the frame's content is written to swap and the frame is repurposed for P. If Q subsequently faults on `uaddr_Q`, its `load_page` attempt will try to acquire `spte_Q->spte_lock`, blocking if P's `evict_frame` still holds it. Once P's `evict_frame` releases `spte_Q->spte_lock` (after updating SPTE_Q to indicate its data is now in swap or needs to be re-read from its original file source), Q's `load_page` will proceed based on this updated SPTE_Q state, typically allocating a *new* physical frame for `uaddr_Q`. The `spte_Q->spte_lock` serializes these operations, preventing Q from re-mapping `uaddr_Q` to the frame while P is still processing its eviction or immediately after P starts using it.

> **B7:** Suppose a page fault in process P causes a page to be read from the file system or swap. How do you ensure that a second process Q cannot interfere by, for example, attempting to evict the frame while it is still being read in?

When process P's `load_page` function allocates a frame via `frame_alloc` (to load data from a file or swap), `frame_alloc` marks the corresponding `frame_table_entry` as `pinned = true`. This `pinned` flag signals that the frame is currently in use for a critical operation (like I/O) and should not be chosen as a victim by the `evict_frame` function. If process Q subsequently triggers a page eviction, my `evict_frame`'s clock algorithm will iterate through the frame table but will skip any frame marked as `pinned`. Once process P's `load_page` successfully completes the data loading (from file or swap) and updates the hardware page table via `install_page`, it then calls `frame_unpin()` (if the `pin` argument to `load_page` was `false`, as is typical for general page faults), which sets `pinned = false` for that frame, making it eligible for future eviction. This pinning mechanism effectively protects the frame during the I/O and setup period.

> **B8:** Explain how you handle access to paged-out pages that occur during system calls. Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design? How do you gracefully handle attempted accesses to invalid virtual addresses?

My system handles access to paged-out user memory during system calls primarily by relying on the **page fault mechanism**, similar to user programs. When a system call handler needs to access user memory (e.g., a buffer pointer or string passed as an argument), helper functions like `check_address`, `get_user`, or `put_user` perform the access. If these functions touch a user virtual address that corresponds to a paged-out page, a page fault is triggered. The page fault handler then invokes `load_page(fault_addr, false)` (the `pin` argument is `false` for a standard fault, meaning the page isn't kept pinned beyond the fault resolution). `load_page` brings the required page into a frame from its backing store (file or swap) and updates the hardware page tables. The system call then resumes. I do not currently implement a mechanism for explicitly "locking" entire user buffers in physical memory for the full duration of a system call. For gracefully handling attempted accesses to invalid virtual addresses by the kernel during a system call: my validation functions (`check_address`, `check_str`) first use `is_user_vaddr()`. If that passes, `check_address` also uses `pagedir_get_page()` and `get_user()`; if any of these checks indicate an invalid address or an unresolvable fault (e.g., `get_user()` returns -1 after `load_page` fails, or `pagedir_get_page` is `NULL` and no fault occurs to fix it for stack arguments), these validation functions call `exit(-1)` to terminate the offending user process.

### Rationale

> **B9:** A single lock for the whole VM system would make synchronization easy but limit parallelism. On the other hand, using many locks complicates synchronization and raises the possibility for deadlock. Explain where your design falls along this continuum and why you chose to design it this way.

*Your answer here.*



---

## Memory Mapped Files

### Data Structures

> **C1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.
```
struct thread{
  struct hash sup_page_table; /* Supplementary page table */
  void *esp;                  /* User sp for page fault */
  struct list mmap_list;      /* List of files mapped to memory */
  mapid_t mapid_cnt;          /* Mapid count */
};
/* The element of list mmap */
struct mmap_file {
  mapid_t mapid;         /* Mapping id */
  struct file *file;     /* File pointer */
  void *base;            /* Base address of the mapped file */
  struct list_elem elem; /* List element */
};

typedef int mapid_t; /*A type for unique memory mapping identifiers.*/

struct sup_page_table_entry {
  void *uaddr;           // User virtual address
  void *kaddr;           // Kernel virtual address
  struct hash_elem elem; // Hash element

  bool writable;           // Is page writable
  enum sup_page_type type; // Type of page

  struct file *file;   // File to load
  off_t offset;        // Offset in file
  uint32_t read_bytes; // Number of bytes to read
  uint32_t zero_bytes; // Number of bytes to zero

  struct lock spte_lock; // Lock for waiting swap
  size_t swap_index;     // Swap index
};
```


### Algorithms

> **C2:** Describe how memory mapped files integrate into your virtual memory subsystem. Explain how the page fault and eviction processes differ between swap pages and other pages.

* **Integration into Virtual Memory:**
    When `mmap` is called, it invokes your `lazy_load` function for each page of the file to be mapped. `lazy_load` creates a Supplemental Page Table Entry (SPTE) which records the user virtual address (`uaddr`), page type (e.g., `PAGE_FILE` or `PAGE_ZERO`), a pointer to the backing `file`, the `offset` within that file, and the `read_bytes`/`zero_bytes` for that page. These SPTEs are stored in the thread's supplemental page table (a hash table). No physical frames are allocated at `mmap` time; data is loaded on demand (lazily) when a page fault occurs.

* **Page Fault Process - Key Data Source Differences:**
    * **Swapped-Out Pages:** My `load_page` calls `swap_in` to retrieve content from the **swap disk slot** indicated in their Supplemental Page Table Entry (SPTE).
    * **Memory-Mapped File (MMF) Pages (`PAGE_FILE` type, not in swap):** My `load_page` reads data directly from their **original backing file**, using location data from the SPTE.
    * **New Anonymous Pages (e.g., `PAGE_ZERO`, initial `PAGE_STACK`):** These are typically provided a **fresh, zero-filled physical frame** rather than reading from a disk source.

* **Eviction Process - Uniform Destination:**
    * When my `evict_frame` function selects any victim page (be it MMF, stack, etc.), I clear its hardware page table mapping and update its SPTE. Then, I consistently call `swap_out()` to write the page's content to a **slot on the swap disk**, recording the `swap_index` in the SPTE.
    * **Key Difference Implied:** My current eviction strategy sends all page types to the swap disk. This contrasts with common alternative strategies where dirty MMF pages might be written back to their original file and clean MMF pages simply discarded, reserving swap primarily for anonymous data.



> **C3:** Explain how you determine whether a new file mapping overlaps any existing segment.

To determine if a new file mapping, defined by a starting virtual `addr` and `file_size`, overlaps with an existing segment, my `mmap` system call typically invokes a helper function `check_overlaps(addr, file_size)`. This helper function iterates through each virtual page that the proposed new mapping would occupy. For every page address in this range, it queries my supplemental page table (SPT) by calling `find_spte()` and may also check the hardware page tables directly using `pagedir_get_page()`. If either of these checks indicates that any page within the proposed range is already in use (e.g., `find_spte()` returns an existing entry or `pagedir_get_page()` shows a current mapping), `check_overlaps` signals that an overlap has been detected (conventionally by returning `false`), which subsequently causes the main `mmap` system call to fail.


### Rationale

> **C4:** Mappings created with `mmap` have similar semantics to those of data demand-paged from executables, except that `mmap` mappings are written back to their original files, not to swap. This implies that much of their implementation can be shared. Explain why your implementation either does or does not share much of the code for the two situations.

In my implementation, memory-mapped files (MMF) and demand-paged data from executables **highly share core mechanisms**: both are managed using a common Supplemental Page Table Entry (SPTE) structure and utilize similar logic via my `lazy_load` function for their initial setup. Consequently, when a page fault occurs, my `load_page` function handles loading data from the backing file in a unified manner for both. **The primary divergence lies in the page eviction strategy:** while the ideal semantic for MMF dirty pages is to be written back to their original file upon eviction, my current `evict_frame` function uniformly writes all types of victim pages, including MMF pages, to the swap disk via `swap_out()`. However, the logic for writing dirty MMF pages back to their original file *is* already implemented within my `munmap` function, presenting an opportunity for this code to be reused or refactored into `evict_frame` in the future to enable a more differentiated eviction policy specifically for MMF pages, aligning more closely with the described semantics.


---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

*Your answer here.*
