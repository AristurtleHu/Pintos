# CS 130: Project 1 - Threads Design Document

## Group Information

Please provide the details for your group below, including your group's name and the names and email addresses of all members.

- **Group Name**: Boeing
- **Member 1**: Renyi Yang `<yangry2023@shanghaitech.edu.cn>`
- **Member 2**: Jiaxing Wu `<wujx2023@shanghaitech.edu.cn>`

---

## Preliminaries

> If you have any preliminary comments on your submission, notes for the TAs, or extra credit, please give them here.

None.

---

## Alarm Clock

### Data Structures

> **A1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

```C
static struct heap sleep_heap; // time.c

struct thread { // thread.h
    int64_t wake_time; 
}
```
For each thread, add a wake_time to record when to awake it. Generate a heap to sort sleeping threads by wake_time.

### Algorithms

> **A2:** Briefly describe what happens in a call to `timer_sleep()`, including the effects of the timer interrupt handler.

Calculate the wake_time of the thread and push the thread into the heap. When a time interrupt occur, check whether to awake the top of the heap (loop until wake_time > now)

> **A3:** What steps are taken to minimize the amount of time spent in the timer interrupt handler?

Using a heap sorted by wake_time, so the earliest thread is always on the top. During awaking, only need to check whether the top should be awaked or not. 

### Synchronization

> **A4:** How are race conditions avoided when multiple threads call `timer_sleep()` simultaneously?

```C
enum intr_level old_level = intr_disable();
// critical section
intr_set_level(old_level);
```
When interrupts are disabled, the CPU cannot be preempted to run another thread, effectively creating an atomic section of code.

> **A5:** How are race conditions avoided when a timer interrupt occurs during a call to `timer_sleep()`?

```C
enum intr_level old_level = intr_disable();
// critical section
intr_set_level(old_level);
```
The interrupt disabling ensures all these operations are atomic from the perspective of the timer interrupt handler.

### Rationale

> **A6:** Why did you choose this design? In what ways is it superior to another design you considered?

Using heap, `O(logn)` push, quicker than directly `list_push`

---

## Priority Scheduling

### Data Structures

> **B1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

```C
struct thread {
  int priority;              /* Priority. */
  int priority_original;     /* Original priority */
  int ord;                   /* FIFO order */
 
  /* Shared between thread.c and synch.c. */
  struct list locks;         /* List of acquired locks */
  struct lock *lock_waiting; /* Waiting Lock */
};

struct lock {
  struct list_elem elem;      /* List element. */
  int priority_donate;        /* Max priority from priority donation */
};
```
thread donate to lock to thread, original used to recover after donation.
Record locks acquired and wait for.

> **B2:** Explain the data structure used to track priority donation. Describe how priority donation works in a nested scenario using a detailed textual explanation.

donation:
1. thread-->lock_waiting->lock-->holder->thread-->lock_waiting->...
2. thread-->locks->lock-->semaphor-->waiters->thread-->locks->...

```C
void donate_priority(struct thread *t, int dep) {
  if (dep > DONATE_MAX_DEPTH || t->lock_waiting == NULL)
    return;

  if (t->lock_waiting->priority_donate < t->priority) {
    t->lock_waiting->priority_donate = t->priority;

    if (t->lock_waiting->holder->priority < t->priority)
      t->lock_waiting->holder->priority = t->priority;

    donate_priority(t->lock_waiting->holder, dep + 1);
  }
}
```

### Algorithms

> **B3:** How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?

`list_max` using `sema_priority_less`(in condition), `lock_priority_less`(lock) and `thread_priority_less`(in sema)

> **B4:** Describe the sequence of events when a call to `lock_acquire()` causes a priority donation. How is nested donation handled?

1. The thread attempts to acquire the lock through the semaphore
2. If the lock is available, the thread becomes the lock holder and continues execution
3. If the lock is unavailable (held by another thread), priority donation occurs:

**Priority Donation Sequence**

- The current thread (high priority) discovers the lock is held by another thread (low priority)
- The high-priority thread "donates" its priority to the lock holder
- The priority_donate field in the lock structure is updated to track this donation
- The current thread blocks on the semaphore until the lock becomes available
 
**Nested Donation Handling**

Nested donation occurs when a chain of locks and threads is involved:

- Thread H (high priority) needs lock L1 held by thread M (medium priority)
- Thread M is waiting for lock L2 held by thread L (low priority)
 
Priority donation cascades through the chain:
1. H donates to M (raising M's priority to H's level)
2. M's new priority is then donated to L (raising L's priority to H's level)

This ensures that thread L runs at the highest priority of any thread waiting on it directly or indirectly, preventing priority inversion across lock chains.

The priority_donate field in each lock stores the highest donated priority, enabling the system to track and correctly revert priorities when locks are released.

> **B5:** Describe the sequence of events when `lock_release()` is called on a lock that a higher-priority thread is waiting for.
> 
The lock's holder field is set to NULL.

The donated priority in priority_donate is likely reset to `PRI_MIN`.

`sema_up(&lock->semaphore)` is called to release the semaphore.
The sema_up() call increments the semaphore value and wakes up one waiting thread with the highest priority.

This demonstrates priority scheduling in action - as soon as a higher-priority thread can run, it takes over from the lower-priority thread.

The donated priority of the thread is changed to the original one.

### Synchronization

> **B6:** Describe a potential race in `thread_set_priority()` and explain how your implementation avoids it. Can you use a lock to avoid this race?

During priority donation, the lock holder’s priority may be set by it’s donor,
at the mean time, the thread itself may want to change the priority.
If the donor and the thread itself set the priority in a different order, may 
cause a different result. 
 
We disable the interrupt to prevent it happens. It can not be avoided using a lock
in our implementation, since we didn’t provide the interface and structure to 
share a lock between donor and the thread itself. If we add a lock to the thread 
struct, it may be avoided using it.

### Rationale

> **B7:** Why did you choose this design? In what ways is it superior to another design you considered?

Easy to track the donation and find the highest priority.

---

## Advanced Scheduler

### Data Structures

> **C1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

*Your answer here.*

### Algorithms

> **C2:** Suppose threads A, B, and C have nice values 0, 1, and 2. Each has a `recent_cpu` value of 0. Fill in the table below showing the scheduling decision and the priority and `recent_cpu` values for each thread after each given number of timer ticks:

*Fill in the table.*

| Timer Ticks | recent_cpu A | recent_cpu B | recent_cpu C | Priority A | Priority B | Priority C | Thread to Run |
| ----------- | ------------ | ------------ | ------------ | ---------- | ---------- | ---------- | ------------- |
| 0           |              |              |              |            |            |            |               |
| 4           |              |              |              |            |            |            |               |
| 8           |              |              |              |            |            |            |               |
| 12          |              |              |              |            |            |            |               |
| 16          |              |              |              |            |            |            |               |
| 20          |              |              |              |            |            |            |               |
| 24          |              |              |              |            |            |            |               |
| 28          |              |              |              |            |            |            |               |
| 32          |              |              |              |            |            |            |               |
| 36          |              |              |              |            |            |            |               |

> **C3:** Did any ambiguities in the scheduler specification make values in the table uncertain? If so, what rule did you use to resolve them? Does this match the behavior of your scheduler?

*Your answer here.*

> **C4:** How is the way you divided the cost of scheduling between code inside and outside interrupt context likely to affect performance?

*Your answer here.*

### Rationale

> **C5:** Briefly critique your design, pointing out advantages and disadvantages in your design choices. If you were to have extra time to work on this part of the project, how might you choose to refine or improve your design?

*Your answer here.*

---

> **C6:** The assignment explains arithmetic for fixed-point math in detail, but it leaves it open to you to implement it. Why did you decide to implement it the way you did? If you created an abstraction layer for fixed-point math (i.e., an abstract data type and/or a set of functions or macros to manipulate fixed-point numbers), why did you do so? If not, why not?

*Your answer here.*

---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

None.
