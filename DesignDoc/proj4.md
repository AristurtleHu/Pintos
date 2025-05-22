# CS 130: Project 4 - File Systems Design Document

## Group Information

Please provide the details for your group below, including your group's name and the names and email addresses of all members.

- **Group Name**: *[Enter your group name here]*
- **Member 1**: FirstName LastName `<email@shanghaitech.edu.cn>`
- **Member 2**: FirstName LastName `<email@shanghaitech.edu.cn>`



---

## Preliminaries

> If you have any preliminary comments on your submission, notes for the TAs, or extra credit, please give them here.

*Your answer here.*



---

## Indexed and Extensible Files

### Data Structures

> **A1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



> **A2:** What is the maximum size of a file supported by your inode structure? Show your work.

*Your answer here.*



### Synchronization

> **A3:** Explain how your code avoids a race if two processes attempt to extend a file at the same time.

*Your answer here.*



> **A4:** Suppose processes A and B both have file F open, both positioned at end-of-file. If A reads and B writes F at the same time, A may read all, part, or none of what B writes. However, A may not read data other than what B writes (e.g., if B writes nonzero data, A is not allowed to see all zeros). Explain how your code avoids this race.

*Your answer here.*



> **A5:** Explain how your synchronization design provides "fairness." File access is "fair" if readers cannot indefinitely block writers or vice versa—meaning that many readers do not prevent a writer, and many writers do not prevent a reader.

*Your answer here.*



### Rationale

> **A6:** Is your inode structure a multilevel index? If so, why did you choose this particular combination of direct, indirect, and doubly indirect blocks? If not, why did you choose an alternative inode structure, and what advantages and disadvantages does your structure have compared to a multilevel index?

*Your answer here.*



---

## Subdirectories

### Data Structures

> **B1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



### Algorithms

> **B2:** Describe your code for traversing a user-specified path. How do traversals of absolute and relative paths differ?

*Your answer here.*



### Synchronization

> **B4:** How do you prevent races on directory entries? For example, only one of two simultaneous attempts to remove a single file should succeed, as should only one of two simultaneous attempts to create a file with the same name.

*Your answer here.*



> **B5:** Does your implementation allow a directory to be removed if it is open by a process or if it is in use as a process's current working directory?  
> If so, what happens to that process's future file system operations? If not, how do you prevent it?

*Your answer here.*



### Rationale

> **B6:** Explain why you chose to represent the current directory of a process the way you did.

*Your answer here.*



---

## Buffer Cache

### Data Structures

> **C1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



### Algorithms

> **C2:** Describe how your cache replacement algorithm chooses a cache block to evict.

*Your answer here.*



> **C3:** Describe your implementation of write-behind.

*Your answer here.*



> **C4:** Describe your implementation of read-ahead.

*Your answer here.*



### Synchronization

> **C5:** When one process is actively reading or writing data in a buffer cache block, how are other processes prevented from evicting that block?

*Your answer here.*



> **C6:** During the eviction of a block from the cache, how are other processes prevented from attempting to access the block?

*Your answer here.*



### Rationale

> **C7:** Describe a file workload likely to benefit from buffer caching, and workloads likely to benefit from read-ahead and write-behind.

*Your answer here.*



---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

*Your answer here.*

