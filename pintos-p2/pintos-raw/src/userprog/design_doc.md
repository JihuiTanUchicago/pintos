```markdown
# Data Structures

## Stack
- Stack requires no extra data structure.

## Deny Write
- Deny write needs to add:
  ```c
  struct file* executable;
  ```
  inside thread's struct

# Algorithms

## Stack
- Inside `process_execute` parse out the filename from the command line and pass the filename to the correct functions.
- Used function `return_filename_only` to make a copy of the filename.
- Do the same thing inside `start_process`. The whole command line will go inside the load.
- `setup_stack` is then called inside this function.
- Inside `setup_stack`, we first check the length of the command is smaller than the page size.
- Next, malloc a list of strings to hold the commands so we can place them onto the stack later.
- Now, if the page is successfully allocated, we start to put the arguments onto the stack.
- We start from the end of the list, while putting each one on there, we also record the total length taken and decrement the `esp`.
- Check the remainder to add a buffer of zeros for alignment.
- Use the same method to add in the rest of the addresses and `argc`.
- Always use `memcpy()` to put things onto the stack.

## Synchronization

### Stack
- No need to sync here.

## Rationale

### Stack
- The video shared by Danial is pretty clear. We want to use the pointer to `memcpy` all the arguments onto the stack.
- The executable file is not the same as other files, so we need to treat it separately.

# Syscall Implementation (except `deny_write()`)

## Attributes Added for Syscall and for Synchronization

- `MAX_CHILDREN` is a `#typedef` integer value defined globally in `thread.h`, it is used for creating arrays needed in `struct thread`. Basically, it marks the maximum children a parent could create. `MAX_CHILDREN` could grow and the size of the array could grow, but we leave this as a future work.
  
- We added a `file_descriptor` struct in `process.c`, where each `file_descriptor` has a corresponding file and the file’s unique `fd` number. We used this struct to keep track of how many files the current thread are managing, and in the `exit` syscall, we use `fd_list` to make sure all files are closed properly.
  
  ```c
  struct list fd_list;
  int fd_num; //for assigning fd number
  ```

- `int exit_code`: this integer value stores the exit status of the current thread. It is used when passing the current thread’s exit status to its parent.
  
- `int exit_child_waited`: this integer value marks if the child when parent calls `wait` has been waited or not. It is specifically used in `process_wait`, where `exit_child_waited` is set to 1 to mark the child has been waited. The attribute will immediately be set back to 0 when the wait is over. There is no point in having a list of such integers to mark each child has been waited or not, since parent could only wait for 1 child at a given time.
  
- `tid_t current_child_tid`: this integer marks the `tid` of the current child the parent is calling `process_execute` with. We need this attribute to correctly match the child with a corresponding semaphore in `exit_child_sema_arr`.

  ```c
  int exit_child_code_arr[MAX_CHILDREN];
  tid_t exit_child_tid_arr[MAX_CHILDREN];
  struct semaphore exit_child_sema_arr[MAX_CHILDREN];
  ```

- `struct thread* parent`: this attribute is for child to know who their parent is.

## How `process_wait()`, `process_exit()`, `process_execute()`, `start_process()` interact with each other in terms of semaphore

- `process_execute`: `sema_down` the semaphore `exit_child_sema_arr[current_child_tid]`, to wait for child to load their stacks and return their `tid`
  
- `start_process`: `sema_up` given the `tid` no matter `setup_stack` succeeds or not because parent needs to be woken up
  
- `process_wait`: check `exit_child_code_arr[child_tid]` and other relevant information to determine if the child is valid or has exited or not. `sema_down` to wait for the child.
  
- `process_exit`: update its exit status code and other relevant information into parent’s corresponding arrays and `sema_up` to let the parent wake up if the parent has been waited

## Deny Write

- Inside `load`, the file opened by `filesys_open` is actually an executable file, which is different from regular read and write file.
  
- Therefore, we deny any write to this file.
  
- In `process_exit()`, close this file so that no later processes can modify this file.

- No need to sync here.
```

