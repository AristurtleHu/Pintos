# Pintos

Pintos is a simple operating system framework designed for the 80x86 architecture. This educational platform serves as a foundation for implementing and understanding key OS concepts. 

[More details](https://web.stanford.edu/class/cs140/projects/pintos/pintos.html)

## Overview

Pintos initially supports:

- Kernel threads
- Loading and running user programs
- A basic file system

These features are implemented in a minimal, straightforward way. The projects progressively enhance Pintos in these areas, including support for virtual memory.

## Project Info

- **Project 1:** Implemented in the `project1` branch
- **Projects 2–4:** Implemented in the `main` branch

### Project 1: Threads and Synchronization

1. Wake threads at the correct time using a heap (avoiding busy waiting)
2. Priority-based scheduling with locks and priority donation
3. Multilevel Feedback Queue Scheduling

### Project 2: User Program and Syscalls

1. Program exit status reporting
2. Argument passing to user programs
3. Basic and Filesystem syscalls

### Project 3: Virtual Memory

1. Page, Page Table, Frame, Swap in/out (Page Fault)
2. Lazy Load and Stack Growth
3. Memory mapping files

### Project 4: File System

1. Indexed and Extensible Files
2. Subdirectories
3. Buffer Cache

## Execution Environment

While Pintos could theoretically run on a standard IBM-compatible PC, for practical reasons, it runs in system simulators that accurately emulate an 80x86 CPU and its peripheral devices. The supported simulators include:

- Bochs
- QEMU
- VMware Player
