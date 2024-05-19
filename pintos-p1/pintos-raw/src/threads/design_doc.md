# Team Contribution
- Jihui("Todd") Tan ~ 49%
- Haoran("Mike") Wu ~ 51%

**All Test Passed**

# Alarm Clock Implementation

The implementation of the alarm clock is intuitive. It's given that we need a list, then there must be an elem for that list in order to use the list.h properly. We see the smaller the wake-up time the earlier a thread should wake-up. We must consider the cases where not only one but multiple threads to wake up from the wait list. We also see that a semaphore is needed inside thread struct in order to let it sleep.

## Data Structures

static struct list wait_list;		/* the waitlist is used to put the threads on there by their wake up time. */

struct list_elem wait_elem;		/* elem for wait_list in timer.c, each list must have an elem. */

int64_t wakeup_time;		/* the wakeup_time for timer_sleep. */

struct semaphore timer_semaphore;	/* added as needed, using it to run semaphore on the thread, in order to put a thread to sleep. */

## Algorithms

The main changes of alarm clock happen in timer.c. Mainly two functions: timer_sleep and timer_interrupt. 
The logic go as follows:
> create a global wait_list. The list is sorted by the wake-up time from the smallest wake-up time to the largest wake-up time (because the larger the wake-up time the later it wakes up). We can sort this list through the function sort_by_wakeup_time through wait_elem inside the thread's struct. We get the wake-up time by adding the start and the ticks. Next, while adding the thread to the list, we must turn off the interrupt so that no two threads can modify the list concurrently. At last, inside timer_sleep, we call sema_down on the thread's semaphore so that the thread goes to sleep. Also, we initialize the sema with value 0, so that when we sema down, the thread sleeps. 

Inside timer_interrupt, we do the following. If the wake-up time is bigger than the ticks we have right now, we must then wake it up so that it is not on the wait list anymore. We iterate through the list and find such threads, and call sema up on them so that they are now ready instead of blocked. 

## Synchronization

The first thing we make sure that the list's content is uncorrupted is by calling interrupt off so that no two threads can modify the wait list, a global variable, simultaneously. The other aspect of synchronization is sema down and sema up on a thread. This is an important part because this is the way we put a thread to sleep such that the thread goes to sleep. 

# Priority Schedule Implementation:

Our Priority Schedule implementation is based on the following mechanism:

## Struct Thread:

Inside Struct Thread, we added 2 additional attributes under Struct Thread:

1)list* locks_I_hold: this keeps track of what locks the current thread has acquired but not released yet. This stores a list of locks.

2)lock* lock_I_want: this keeps track of what lock the current thread is waiting for. We only need to remember one lock, since if the thread wants to acquire a lock but failed because another thread is holding it, then the thread would go to sleep. Therefore, it is impossible to have more than 1 lock that a thread wants at a given time.

3)int cur_priority: this is the priority used for priority scheduling. when thread A donates to thread B, B's priority would not change, but B's cur_priority would change to A's cur_priority. The thread's original(real) priority only changes when it tries to change the priority itself.

## Struct Lock:

Correspondingly, we added list_elem* elem in lock, so that it could be added to locks_I_hold in struct thread

## Priority Donation:

When Thread A donates to Thread B it's cur_priority, if Thread B has a lock it wants, Thread A would also have to donate to the lock's holder Thread C if A has greater cur_priority. This process repeats until Thread A's cur_priority is no longer the bigger one or Thread X does not have a lock it wants.

By recursively doing donation, it prevents situations such as B wants C's lock, but B has higher cur_priority than C, which causes scheduler to not run C at all, which means B can never acquire lock because C never releases the lock.

## lock_acquire():

When a thread A calls lock_acquire to acquire a lock X, it's all fine if X currently does not have a holder. If X is acquired by someone else, A would have to check whether it's priority is greater than X's holder and do donation if necessary. After that, A would be put to sleep until the lock is available again.

## lock_release():

When a thread A releases a lock X by calling lock_release(), it will remove X from it's locks_I_hold list and restore its cur_priority to its original priority(attribute priority), or to the highest priority among A's holding locks' holders, whichever is bigger and depend on whether the latter exists(locks_I_hold maybe NULL or empty or holding locks don't actually have holders).

## thread_yield():

When should the current thread gives up CPU voluntarily? We considered several cases:

1)*thread_create*: If we are creating a new thread that has higher cur_priority than the current running thread, it makes sense to yield and schedule a higher priority thread to run

2)*thread_set_priority*: If we are updating the priority & cur_priority of the current thread, it should yield to make scheduler reschedule, unless the interrupt has been disabled, as in lock_release. Scheduling when interrupt is disabled would cause OS to not make threads run in correct order.

3)*sema_up*: If a thread is waking up, it may have higher priority than the current thread, so we better call thread_yield to let scheduler to reschedule.

## Priority Schedule Example

For the following example:

- the notation "A:31, [a], b" would mean that Thread A has priority 31, lock a, and wants to acquire lock b. 
- A:31 -> B:30 => A,B:31. This means Thread A donates to thread B, and A,B now have cur_prioirty 31.


Now: A:31, [b]; B:32, [c], b; C:33;

B wants to acquire lock b, and c wants to acquire lock c.

Schedule, C:33 runs, failed to acquire lock c, look at c->holder = B, donate, then sleep.

C:33 -> B:32 => C,B:33. Recursively donate, B look at b->holder = A, donate, then sleep.

B:33 -> A:31 => B,A:33. Now only A->status == THREAD_READY, A runs and finished.

Lock b released -> A restores back to cur_priority = 31 via thread_set_priority()

sema_up() -> look at lock b -> semaphore -> waiters, B popped out and wakes up, runs, finished

Say, lock b released first. look at lock b->sempahore->waiters, nothing. Yield and B still runs.

lock c released. Look at lock c->semaphore->waiters, C popped out and wakes up, runs, finished




