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

// heap.h
typedef void *heap_elem;

typedef bool heap_less_func(const heap_elem a, const heap_elem b);

struct heap {
  size_t size;                    /* Heap size. */
  heap_elem elems[MAX_HEAP_SIZE]; /* Heap elements. */
  heap_less_func *less;           /* Heap compare function. */
};
```
1. For each thread, add a wake_time to record when to awake it. 

2. Generate a heap to sort sleeping threads by wake_time and wake up then when time interrupts.

3. A max-heap template of elements based on heap_less_func

### Algorithms

> **A2:** Briefly describe what happens in a call to `timer_sleep()`, including the effects of the timer interrupt handler.

Calculate the wake_time of the thread (`ticks + sleep`) and push the thread into the heap. When a time interrupt occur, check whether to awake the top of the heap (loop until wake_time > now)

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

Using heap, `push` operation cost `O(logn)`. During waking up, popping from heap costs `O(logn)` per thread. This is much more efficient than scanning through the entire list of sleeping threads on every timer tick, which would be `O(n)` where n is the number of sleeping threads.

The heap-based approach also keeps the timer interrupt handler running quickly since we only need to check the thread at the top of the heap. We only perform wake-up operations when absolutely necessary.

Alternative designs we considered is sorted list, but it would require `O(n)` for insertion.

---

## Priority Scheduling

### Data Structures

> **B1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

```C
struct thread {
  /* Owned by thread.c. */
  tid_t tid;                 /* Thread identifier. */
  enum thread_status status; /* Thread state. */
  char name[16];             /* Name (for debugging purposes). */
  uint8_t *stack;            /* Saved stack pointer. */
  int priority;              /* Priority. */
  int priority_original;     /* Original priority */
  int64_t wake_time;         /* Wake up time */
  int ord;                   /* FIFO order */
  struct list_elem allelem;  /* List element for all threads list. */
  int nice;                  /* Nice value for MLFQS */
  fixed_t recent_cpu;        /* Recent CPU for MLFQS */

  /* Shared between thread.c and synch.c. */
  struct list_elem elem;     /* List element. */
  struct list locks;         /* List of acquired locks */
  struct lock *lock_waiting; /* Waiting Lock */
};

struct lock {
  struct thread *holder;      /* Thread holding lock (for debugging). */
  struct semaphore semaphore; /* Binary semaphore controlling access. */
  struct list_elem elem;      /* List element. */
  int priority;               /* Max priority from priority donation */
};
```

Donation:
thread `->` lock `->` thread 

`priority_original` used to recover after donation.

Record locks acquired and wait for.

> **B2:** Explain the data structure used to track priority donation. Describe how priority donation works in a nested scenario using a detailed textual explanation.

```C
/* Donate priority.
   If exceed `DONATE_MAX_DEPTH`, then may occur a deadlock */
void donate_priority(struct thread *t, int dep) {
  struct lock *lock = t->lock_waiting;

  if (dep > DONATE_MAX_DEPTH || lock == NULL)
    return;

  if (lock->priority < t->priority) { // Need donation
    // Donate to lock
    lock->priority = t->priority;
    // Donate to lock's holder
    lock->holder->priority = max(t->priority, lock->holder->priority);

    donate_priority(lock->holder, dep + 1);
  }
}
```

Donation:
- thread `->` lock_waiting `==` lock `->` holder `==` thread `->` lock_waiting `==` ...

Example:
```c
1)  | low |         
// a thread with low priority holds a lock
2)  | med | -> | low |              
// a thread with medium priority waits on that lock
3)  | med | -> | med |           
// that thread donates its priority to the previously-low thread
4)  | high | -> | med | -> | med |  
// a high priority thread wants a lock that medium has
5)  | high | -> | high | -> | med |  
// high priority thread donates it's priority to the next thread in the donation chain
6)  | high | -> | high | -> | high |  
// the newly-high thread donates it's new priority to the first thread 
```

### Algorithms

> **B3:** How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?

`list_max` using `sema_priority_less`(in condition), `lock_priority_less`(lock) and `thread_priority_less`(in sema)

When a lock or semaphore is released, we scan the list of waiting threads and wake up the one with the highest priority first. This thread is then removed from the waiters list. For condition variables, we examine the semaphores in the waiters list to identify which one has the highest-priority thread waiting, ensuring that high-priority threads are always serviced before lower-priority ones.

> **B4:** Describe the sequence of events when a call to `lock_acquire()` causes a priority donation. How is nested donation handled?

1. The thread attempts to acquire the lock through the semaphore
2. If the lock is available, the thread becomes the lock holder and continues execution
3. If the lock is unavailable (held by another thread), priority donation occurs:

**Priority Donation Sequence**

- The current thread (high priority) discovers the lock is held by another thread (low priority)
- The high-priority thread "donates" its priority to the lock holder
- The current thread blocks on the semaphore until the lock becomes available
 
**Nested Donation Handling**

Nested donation occurs when a chain of locks and threads is involved:

- Thread H (high priority) needs lock L1 held by thread M (medium priority)
- Thread M is waiting for lock L2 held by thread L (low priority)
 
Priority donation cascades through the chain:
1. H donates to M (raising M's priority to H's level)
2. M's new priority is then donated to L (raising L's priority to H's level)

This ensures that thread L runs at the highest priority of any thread waiting on it directly or indirectly, preventing priority inversion across lock chains.

> **B5:** Describe the sequence of events when `lock_release()` is called on a lock that a higher-priority thread is waiting for.
> 
The lock's holder field is set to NULL.

The donated priority of the lock is reset to `PRI_MIN`.

The donated priority of the thread is changed to the original one or other locks' donated priority it holds.

`sema_up(&lock->semaphore)` is called to release the semaphore.
The sema_up() call increments the semaphore value and wakes up one waiting thread with the highest priority.

### Synchronization

> **B6:** Describe a potential race in `thread_set_priority()` and explain how your implementation avoids it. Can you use a lock to avoid this race?

If the current thread has nothing related with the lock, no race will occur. Then the only possible situation is that the current holds the lock, because others want the lock will be blocked.

During priority donation, the lock holder’s priority may be set by it’s donor, at the mean time, the thread itself may want to change the priority.

However, our donation starts when the current thread `lock_acquire`, which has a protection like `A4`. Avoid this race by disabling interrupts.

We may not use a lock for this purpose because using a lock to protect another lock's priority would create circular dependencies.

### Rationale

> **B7:** Why did you choose this design? In what ways is it superior to another design you considered?

We chose this design for several important reasons:

1. By having threads maintain a list of locks they hold and a pointer to the lock they're waiting for, we create a clear donation chain that can be followed recursively.

2. The `priority_original` field preserves the thread's base priority while allowing its effective priority to change through donation.

3. Using `list_max` with appropriate comparator functions ensures the highest priority thread is always selected first.

4. We limit donation depth with `DONATE_MAX_DEPTH` to avoid infinite recursion in circular donation situations.

5. When a lock is released, we can easily recalculate the thread's priority.

We considered a simpler approach without tracking lock ownership, where we would just temporarily boost priorities but not propagate them through chains. This would be easier to implement but would fail to address nested priority inversion scenarios, where multiple locks create a dependency chain. Our current design robustly handles these complex cases while maintaining reasonable performance characteristics.


---

## Advanced Scheduler

### Data Structures

> **C1:** Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

Add to thread.c:

`typedef int32_t fixed_t`, This defines a new type fixed_t as a 32-bit signed integer (int32_t).

`fixed_t load_avg`, It reflects the overall system load and is used to adjust recent_cpu values for all threads.

Add to struct thread:

`fixed_t recent_cpu` tracks how much CPU time a thread has used "recently".

`int nice`, It is a user-controllable parameter that influences thread priority.


### Algorithms

> **C2:** Suppose threads A, B, and C have nice values 0, 1, and 2. Each has a `recent_cpu` value of 0. Fill in the table below showing the scheduling decision and the priority and `recent_cpu` values for each thread after each given number of timer ticks:

*Fill in the table.*

| Timer Ticks | recent_cpu A | recent_cpu B | recent_cpu C | Priority A | Priority B | Priority C | Thread to Run |
| ----------- | ------------ | ------------ | ------------ | ---------- | ---------- | ---------- | ------------- |
| 0           | 0            | 0            | 0            | 63         | 61         | 59         | A             |
| 4           | 4            | 0            | 0            | 62         | 61         | 59         | A             |
| 8           | 8            | 0            | 0            | 61         | 61         | 59         | B             |
| 12          | 8            | 4            | 0            | 61         | 60         | 59         | A             |
| 16          | 12           | 4            | 0            | 60         | 60         | 59         | B             |
| 20          | 12           | 8            | 0            | 60         | 59         | 59         | A             |
| 24          | 16           | 8            | 0            | 59         | 59         | 59         | C             |
| 28          | 16           | 8            | 4            | 59         | 59         | 58         | B             |
| 32          | 16           | 12           | 4            | 59         | 58         | 58         | A             |
| 36          | 20           | 12           | 4            | 58         | 58         | 58         | C             |

> **C3:** Did any ambiguities in the scheduler specification make values in the table uncertain? If so, what rule did you use to resolve them? Does this match the behavior of your scheduler?

Yes, there are some thread that has the same priority. We just excute those thread in FIFO manner.

It is match, because in thread_yield we push current thread to the back of ready-heap. And we will rebuild the heap, only the previous thread priority is less than next thread, they will swap each other.

> **C4:** How is the way you divided the cost of scheduling between code inside and outside interrupt context likely to affect performance?

Actually, the main part of the code was implemented in thread_ticks, there is a `heap_rebuild` when the priority update `O(n)`, and we get the max priority faster`O(logn)`. We obey the rule of strict priority. 

### Rationale

> **C5:** Briefly critique your design, pointing out advantages and disadvantages in your design choices. If you were to have extra time to work on this part of the project, how might you choose to refine or improve your design?

Advantage:
 Through dynamic adjustments of recent_cpu and nice values, the system can automatically distinguish between CPU-bound and I/O-bound threads, ensuring that the former's priority gradually decreases while the latter quickly regains CPU access.

 The formula `recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice` applies exponential decay to historical CPU time, focusing more on recent behavior. It can imporve the fairness.


Disadvantage:
load_avg is updated only once per second, potentially failing to capture transient load fluctuations promptly.

And updating the cpu_recent and priority is `O(n)`, I do not know if the mlfqs is good for a system with many threads in the ready_heap.

Improvement:
Change the update frequency of load_avg from once per second to every tick, using a sliding window average.

Maybe just ignore the temporary wrong of ready_heap.


---

> **C6:** The assignment explains arithmetic for fixed-point math in detail, but it leaves it open to you to implement it. Why did you decide to implement it the way you did? If you created an abstraction layer for fixed-point math (i.e., an abstract data type and/or a set of functions or macros to manipulate fixed-point numbers), why did you do so? If not, why not?

Yes, we use macros to finish this part. Because, macros eliminate function call, hides low-level details (bit shifts, scaling) behind intuitive macros. To avoids memory and overhead costs of objects/structs; fixed-point math is inherently integer-based. This approach balances efficiency, clarity, and flexibility, leveraging C’s low-level features while providing a safe, scalable abstraction for fixed-point arithmetic.


---

## Survey Questions

> Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects? Any other comments?

None.
