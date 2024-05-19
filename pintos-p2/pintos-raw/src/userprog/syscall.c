#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);
static void copy_in (void *dst_, const void *usrc_, size_t size);
static char* copy_in_string (const char *us);
static inline bool get_user (uint8_t *dst, const uint8_t *usrc);
static inline bool put_user (uint8_t *udst, uint8_t byte);
void exit(int status);
struct file* get_file_by_fd(int fd);
void syscall_init (void);
void halt(void);
int wait (pid_t pid);
pid_t exec (const char *cmd_line);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
static int read (int fd, const void *buffer, unsigned size);
static int write (int handle, const void *usrc_, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

//helper function for getting file given fd
struct file* get_file_by_fd(int fd){
  //DELETE
  //printf("get_file_by_fd\n");
  if(&thread_current()->fd_list == NULL || list_empty(&thread_current()->fd_list)){
    return NULL;
  }
  //looping through fd_list in the thread and find the one that has matched fd
  struct list_elem* e;
  for(e = list_begin(&thread_current()->fd_list); e != list_end(&thread_current()->fd_list); e = list_next(e)){
    struct file_descriptor* file_des = list_entry(e, struct file_descriptor, elem);
    if(file_des->fd == fd){
      return file_des->file;
    }
  }

  //base case: file not found
  return NULL;
}

//init syscall, also init lock here
void syscall_init (void) {
  //DELETE
  //printf("syscall_init\n");
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


// Powering off Pintos
void halt(void){
  //DELETE
  //printf("halt\n");
  shutdown_power_off();
}

//exit the current program, using thread_exit();
void exit(int status){
  //DELETE
  //printf("exit\n");
  //closing all files in fd_list of the current thread before exit
  //iterating through fd_list
  while (!list_empty(&thread_current()->fd_list)) {
    struct list_elem *e = list_pop_front(&thread_current()->fd_list);
    struct file_descriptor* file_des = list_entry(e, struct file_descriptor, elem);

    if (file_des != NULL && file_des->file != NULL) {
      file_close(file_des->file); 
    }
  }

  //record status into exit_code of current thread
  thread_current()->exit_code = status;

  printf ("%s: exit(%d)\n", thread_current()->name, thread_current()->exit_code);
  thread_exit();
}

/*Runs the executable whose name is given in cmd line, 
passing any given arguments, 
and returns the new process’s program id (pid).*/
pid_t exec(const char* cmd_line){
  //DELETE
  //printf("exec\n");
  //edge case handling: cmd_line is NULL
  if (cmd_line == NULL || !is_user_vaddr(cmd_line)){
      return -1;
  }
  //copy from user to kernel space first
  char* kcmd_line = copy_in_string(cmd_line);
  //edge case handling: kcmd_line NULL or empty
  if(kcmd_line == NULL){
    return -1;
  }
  if(*kcmd_line == '\0'){
    palloc_free_page((void*)kcmd_line);
    return -1;
  }

  pid_t pid = process_execute(kcmd_line);
  palloc_free_page((void*)kcmd_line);


  return pid;
}

/*Waits for a child process pid and
retrieves the child’s exit status.*/
int wait(pid_t pid){
  //DELETE
  //printf("wait\n");
  int exit_code = process_wait(pid);
  return exit_code;
}

/* Create system call. */
bool create (const char *ufile, unsigned initial_size)
{
  //DELETE
  //printf("create\n");
  lock_acquire(&file_lock);
  //copying user string to kernel space
  char *kfile = copy_in_string (ufile);
  //check if kfile is valid
  if(kfile == NULL || kfile == '\0'){
    return false;
  }
  bool ok = filesys_create (kfile, initial_size);
  //freeing resources from kfile
  palloc_free_page((void*)kfile);
  lock_release(&file_lock);
  return ok;
}

/*Deletes the file called file. Returns true if successful, 
false otherwise.*/
bool remove (const char *file){
  //DELETE
  //printf("remove\n");
  lock_acquire(&file_lock);
  bool ok = filesys_remove(file);
  lock_release(&file_lock);
  return ok;
}

/* Opens the file called file. Returns a nonnegative 
integer handle called a “file descrip- tor” (fd), or -1 
if the file could not be opened. */
//------------------------------------------------------------------
//WARNING: FLAWS in fd_num since it could reach INT_MAX and overflow
//------------------------------------------------------------------
int open(const char *file){
  //DELETE
  //printf("open\n");
  //copy file name to kernal space
  const char *kfile = copy_in_string(file);
  //check if kfile is NULL or empty
  if(kfile == NULL){
    return -1;
  }

  //process synchronization control
  lock_acquire(&file_lock);
  struct file* f = filesys_open(file);

  lock_release(&file_lock);

  //edge case handling: check if f is NULL
  if(f == NULL){
    return -1;
  }

  //now creating file_des and assigning fd number, push to thread's fd_list
  //added a fd_num for assigning number to fd in thread struct
  struct file_descriptor* file_des = malloc(sizeof(struct file_descriptor));
  //edge case handling: if malloc failed
  if(file_des == NULL){
    lock_acquire(&file_lock);
    file_close(f);
    lock_release(&file_lock);
    return -1;
  }
  
  file_des->file = f; //file f added to its file descriptor
  file_des->fd = thread_current()->fd_num++; //fd number assignment, check thread struct for initilization

  //push file_des to current thread's fd_list
  list_push_back(&thread_current()->fd_list, &file_des->elem);

  palloc_free_page((void*)kfile);
  
  return file_des->fd;
}

/* Returns the size, in bytes, of the file open as fd. */
int filesize (int fd){
   //DELETE
  //printf("filesize\n");
  struct file* f = get_file_by_fd(fd);
  //edge case handling: somehow did not get f given fd
  if(f == NULL){
    return -1; //-1 indicating error
  }

  //lock needed b/c other syscall that may change the length
  lock_acquire(&file_lock);
  int length = file_length(f);
  lock_release(&file_lock);

  return length;
}

/* Read system call. similar to write system call*/
static int read (int fd, const void *buffer, unsigned size)
{
   //DELETE
  //printf("read\n");
  //just return if size is 0
  if (size == 0) {
    return 0;
  }

  //edge case handling: if size is 0, 
  //or buffer is NULL or not a user virtual address
  if(buffer == NULL || !is_user_vaddr(buffer) || !is_user_vaddr((void*)((char*)buffer + size))) {
    exit(-1);
  }

  // Retrieve the file from the file descriptor
  struct file* f = get_file_by_fd(fd);
  if (f == NULL) {
    exit(-1);
  }


  unsigned bytes_read = 0;
  unsigned size_to_read = size;
  while (size_to_read > 0) {
    char *kbuffer = (char*)palloc_get_page(0);
    if (kbuffer == NULL) {
      return bytes_read;
    }

    unsigned read_amount = (size - bytes_read < PGSIZE) ? (size - bytes_read) : PGSIZE;
    
    unsigned retval;
    if (fd == STDIN_FILENO) {
      //for reading from the console
      //input_getc from devices/input.h is helpful
      for (unsigned i = 0; i < read_amount; i++) {
        kbuffer[i] = input_getc();
      }
      retval = read_amount;
    } else {
      lock_acquire(&file_lock);
      retval = file_read(f, kbuffer, read_amount);
      lock_release(&file_lock);
    }

    //copy data from kernel buffer to user buffer
    for (unsigned i = 0; i < retval; i++){
        if (!put_user ((uint8_t *)buffer + bytes_read + i, kbuffer[i]))
          {
            //if reading to user space fails, terminate the process
            palloc_free_page ((void*)kbuffer);
            exit (-1);
          }
    }

    bytes_read += retval;
    size_to_read -= retval;
    palloc_free_page((void*)kbuffer);

    if (retval < read_amount) {
      break; // EOF or error reading from file
    }
  }

  return bytes_read;
}


/* Write system call. */
static int 
write (int handle, const void *usrc_, unsigned size) {
  //DELETE
  //printf("write\n");
    //just return if size is 0
    if (size == 0) {
      return 0;
    }

    struct file* f = get_file_by_fd(handle);
   
    //if no file is associated with the handle
    if (handle != STDOUT_FILENO && f == NULL) {
        exit(-1);
    }

    //terminate the process if `usrc_` is NULL
    if (usrc_ == NULL) {
        exit(-1);
    }

    //check validity of entire buffer range
    for (unsigned i = 0; i < size; i++) {
        void *current_addr = (void *) ((char *) usrc_ + i);
        if (!is_user_vaddr(current_addr)) {
            exit(-1);
        }
    }

    unsigned bytes_written = 0;

    while (bytes_written < size) {
        char *kbuffer = (char*) palloc_get_page(0);
        if (kbuffer == NULL) {
            return bytes_written;
        }

        unsigned write_amount = (PGSIZE > size - bytes_written) ? size - bytes_written : PGSIZE;
        copy_in(kbuffer, (uint8_t*) usrc_ + bytes_written, write_amount);
        unsigned retval;
        if (handle == STDOUT_FILENO) {
            putbuf(kbuffer, write_amount);
            retval = write_amount;
        } else {
            lock_acquire(&file_lock);
            retval = file_write(f, kbuffer, write_amount);
            lock_release(&file_lock);
        }

        bytes_written += retval;
        palloc_free_page((void*) kbuffer);

        //break if actual wrriten amount is less than theoretical amount
        if (retval < write_amount) {
            break;
        }
    }

    return bytes_written;
}

//Changes the next byte to be read or written in open file fd to position, 
//expressed in bytes from the beginning of the file.
void seek (int fd, unsigned position){
   //DELETE
  //printf("seek\n");
  struct file* f = get_file_by_fd(fd);
  //edge case handling if f is NULL
  if(f != NULL){
    lock_acquire(&file_lock);
    file_seek(f,position);
    lock_release(&file_lock);
  }
}

//Returns the position of the next byte to be read 
//or written in open file fd, expressed in bytes from the beginning of the file.
unsigned tell (int fd){
  //DELETE
  //printf("tell\n");
  struct file* f = get_file_by_fd(fd);
  //edge case handling if f is NULL
  if(f == NULL){
    return -1; //indicating error
  }

  //synchronization lock due to potential changes in file
  lock_acquire(&file_lock);
    //get pos via file_tell
    off_t pos = file_tell(f);
  lock_release(&file_lock);

  return pos;
}

//Closes file descriptor fd. Exiting or terminating a process implicitly 
//closes all its open file descriptors, as if by calling this function for each one.
void close (int fd){
  //DELETE
  //printf("close\n");
  //get the file by fd by using the helper function get_file_by_fd
  struct file* f = get_file_by_fd(fd);
  //edge case handling: f not found given fd
  if(f!=NULL){
    //closing the file if f exists
    lock_acquire(&file_lock);
    file_close(f);
  
    //now update the fd_list by removing this closed f
    struct file_descriptor* cur_file_des;
    struct list_elem* e;
    //finding the matched fd to be removed
    for (e = list_begin(&thread_current()->fd_list); 
         e != list_end(&thread_current()->fd_list); 
         e = list_next(e)){
      cur_file_des = list_entry(e, struct file_descriptor, elem);
      if (cur_file_des->fd == fd) {
        list_remove(e);
        free(cur_file_des); //remember to free memory allocated for cur_file_des
        break;
      }
    }
    lock_release(&file_lock);
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* f->esp stack structure overview
                                      +----------------+ 
                      0xbffffe7c      |       3        | 
                      0xbffffe78      |       2        | 
                      0xbffffe74      |       1        | 
  stack pointer -->   0xbffffe70      | return address | 
                                      +----------------+
  */
 //check if the stack pointer is in user space
 //DELETE
  //printf("syscall_handler\n");
 if (!is_user_vaddr(f->esp)) {
    exit(-1);
  }

  //copy syscall number and arguments from user space to kernel space
  unsigned call_nr; //syscall number
  copy_in (&call_nr, f->esp, sizeof call_nr); //get the system call number and store in call_nr
  int args[3]; // It's 3 because that's the max number of arguments in all syscalls.
  memset (args, 0, sizeof args);


  // copy the args (depends on arg_cnt for every syscall).
  // note that if the arg passed is a pointer (e.g. a string),
  // then we just copy the pointer here, and you still need to
  // call 'copy_in_string' on the pointer to pass the string
  // from user space to kernel space
  int arg_cnt = 0;
  //look at call_nr and match the sys number with the right sys call
  //TODO: need to check if user arguments contain NULL/empty/attempt to access kernel space 
  switch(call_nr){
    case SYS_HALT: 
      halt();
      break;
    case SYS_EXIT: 
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      exit((int)args[0]);
      break;
    case SYS_EXEC: 
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = exec((const char*)args[0]);
      break;
    case SYS_WAIT: 
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = wait((pid_t)args[0]);
      break;
    case SYS_CREATE:
      arg_cnt = 2;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = create((const char*)args[0], (unsigned)args[1]);
      break;
    case SYS_REMOVE: 
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = remove((const char*)args[0]);
      break;
    case SYS_OPEN: 
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = open((const char*) args[0]);
      break;
    case SYS_FILESIZE:  
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = filesize((int)args[0]);
      break;
    case SYS_READ:  
      arg_cnt = 3;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = read((int)args[0], (const void*) args[1], (unsigned) args[2]);
      break;
    case SYS_WRITE:
      arg_cnt = 3;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = write((int)args[0], (const void*) args[1], (unsigned) args[2]);
      break;
    case SYS_SEEK: 
      arg_cnt = 2;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      seek((int)args[0], (unsigned)args[1]);
      break;
    case SYS_TELL: 
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      f->eax = tell((int)args[0]);
      break;
    case SYS_CLOSE: 
      arg_cnt = 1;
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * arg_cnt);
      close((int)args[0]);
      break;
    //error handling for unknown syscall
    default: 
      exit(-1);
  }
}

/* Copies SIZE bytes from user address USRC to kernel address DST.  Call
   thread_exit() if any of the user accesses are invalid. */ 
static void copy_in (void *dst_, const void *usrc_, size_t size) { 
  //DELETE
  //printf("copy_in\n");
  uint8_t *dst = dst_; 
  const uint8_t *usrc = usrc_;
  
  for (; size > 0; size--, dst++, usrc++)
    if (usrc >= (uint8_t *) PHYS_BASE || !get_user (dst, usrc)){
      exit(-1);
    }
}

/* Creates a copy of user string US in kernel memory and returns it as a
   page that must be **freed with palloc_free_page()**.  Truncates the string
   at PGSIZE bytes in size.  Call thread_exit() if any of the user accesses
   are invalid. */
static char *
copy_in_string (const char *us)
{
  //DELETE
  //printf("copy_in_string\n");
  if (us == NULL || !is_user_vaddr(us)){
    exit(-1);
  }

  char *ks = (char*)palloc_get_page (0);
  if (ks == NULL){
    exit(-1);
  }

  //read bytes by bytes
  for (size_t i = 0; i < PGSIZE; i++) 
    {   
      // call get_user() until you see '\0'
      uint8_t byte;
      //error handling if unable to read from uaddr
      //or partial access to kernel, which should
      //not be validate
      if(!get_user(&byte, (uint8_t *)(us + i))){
        palloc_free_page((void*)ks);
        exit(-1);
      }

      // copying from user space to kernel space
      memcpy(ks+i, &byte, sizeof(uint8_t));
      if((char)byte == '\0'){
        break;
      }
      
      //make sure ks is null-terminated if PGSIZE is reached
      if(i == PGSIZE){
        ks[PGSIZE-1] = '\0';
      }
    }

  return ks;
  // don't forget to call palloc_free_page(..) when you're done
  // with this page, before you return to user from syscall
}

/* Copies a byte from user address USRC to kernel address DST.  USRC must
   be below PHYS_BASE.  Returns true if successful, false if a segfault
   occurred. Unlike the one posted on the p2 website, this one takes two
   arguments: dst, and usrc */
static inline bool get_user (uint8_t *dst, const uint8_t *usrc){
  int eax;
  asm ("movl $1f, %%eax; movb %2, %%al; movb %%al, %0; 1:"
       : "=m" (*dst), "=&a" (eax) : "m" (*usrc));
  return eax != 0;
}


/* Writes BYTE to user address UDST.  UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static inline bool put_user (uint8_t *udst, uint8_t byte){

  int eax;
  asm ("movl $1f, %%eax; movb %b2, %0; 1:"
       : "=m" (*udst), "=&a" (eax) : "q" (byte));
  return eax != 0;
}