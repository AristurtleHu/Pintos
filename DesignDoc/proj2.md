# CS 130: Project 2 - User Programs Design Document

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

## Argument Passing

### Data Structures

> **A1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



### Algorithms

> **A2:** Briefly describe how you implemented argument parsing. How do you arrange for the elements of `argv[]` to be in the right order? How do you avoid overflowing the stack page?

*Your answer here.*



### Rationale

> **A3:** Why does Pintos implement `strtok_r()` but not `strtok()`?

*Your answer here.*



> **A4:** In Pintos, the kernel separates commands into an executable name and arguments, while Unix-like systems have the shell perform this separation. Identify at least two advantages of the Unix approach.

*Your answer here.*



---

## System Calls

### Data Structures

> **B1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*



> **B2:** Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?

*Your answer here.*



### Algorithms

> **B3:** Describe your code for reading and writing user data from the kernel.

*Your answer here.*



> **B4:** Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel. What is the least and the greatest possible number of inspections of the page table (e.g., calls to `pagedir_get_page()`) that might result? What about for a system call that only copies 2 bytes of data? Is there room for improvement in these numbers, and how much?

*Your answer here.*



> **B5:** Briefly describe your implementation of the "wait" system call and how it interacts with process termination.

*Your answer here.*



> **B6:** Accessing user program memory at a user-specified address may fail due to a bad pointer value, requiring termination of the process. Describe your strategy for managing error-handling without obscuring core functionality and ensuring that all allocated resources (locks, buffers, etc.) are freed. Give an example.

*Your answer here.*



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

*Your answer here.*



---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

*Your answer here.*
