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
#include "vm/page.h"

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
static int write (int handle, void *usrc_, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

//helper function for getting file given fd
struct file* get_file_by_fd(int fd){
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
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


// Powering off Pintos
void halt(void){
  shutdown_power_off();
}

//exit the current program, using thread_exit();
void exit(int status){
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
  //edge case handling: cmd_line is NULL
  pid_t pid;
  char *kfile = copy_in_string (cmd_line);

  lock_acquire (&file_lock);
  pid = process_execute (kfile);
  lock_release (&file_lock);

  palloc_free_page (kfile);
  return pid;
}

/*Waits for a child process pid and
retrieves the child’s exit status.*/
int wait(pid_t pid){
  int exit_code = process_wait(pid);
  return exit_code;
}

/* Create system call. */
bool create (const char *ufile, unsigned initial_size)
{
  lock_acquire(&file_lock);
  char *kfile = copy_in_string (ufile);//copying user string to kernel space
  if(kfile == NULL || kfile == '\0'){return false;}//check if kfile is valid
  bool ok = filesys_create (kfile, initial_size);
  //freeing resources from kfile
  palloc_free_page((void*)kfile);
  lock_release(&file_lock);
  return ok;
}

/*Deletes the file called file. Returns true if successful, 
false otherwise.*/
bool remove (const char *file){
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
  const char *kfile = copy_in_string(file);//copy file name to kernal space
  if(kfile == NULL){return -1;}//check if kfile is NULL or empty

  //process synchronization control
  lock_acquire(&file_lock);
  struct file* f = filesys_open(file);
  lock_release(&file_lock);

  if(f == NULL){
    //printf("f is NULL\n");
    return -1;
  }//edge case handling: check if f is NULL

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
  struct file* f = get_file_by_fd(fd);
  if(f == NULL){return -1;} //edge case handling: somehow did not get f given fd

  //lock needed b/c other syscall that may change the length
  lock_acquire(&file_lock);
  int length = file_length(f);
  lock_release(&file_lock);

  return length;
}

/* Read system call. similar to write system call*/
static bool validate_read_parameters(const void *buffer, unsigned size) {
    struct page* pg = page_for_addr(buffer);
    return buffer != NULL && 
           is_user_vaddr(buffer) && 
           is_user_vaddr((void*)((char*)buffer + size)) &&
           pg != NULL &&
           !pg->read_only;
}

static int perform_file_read(int fd, struct file* file, uint8_t *dest, size_t size) {
    int total_read = 0;
    off_t result;
    while (size > 0) {
        size_t page_offset = pg_ofs(dest);
        size_t read_amount = size < PGSIZE - page_offset ? size : PGSIZE - page_offset;
        
        if (!page_lock(dest, true))
            thread_exit();
        lock_acquire(&file_lock);
        result = file_read(file, dest, read_amount);
        lock_release(&file_lock);
        page_unlock(dest);

        if (result < 0) {
            return total_read > 0 ? total_read : -1;
        }
        total_read += result;
        if (result != (off_t) read_amount) {
            break; // Short read, so we're done.
        }

        dest += result;
        size -= result;
    }
    return total_read;
}

static int perform_stdin_read(uint8_t *dest, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        char c = input_getc();
        if (!page_lock(dest, true))
            thread_exit();
        dest[i] = c;
        page_unlock(dest);
    }
    return size;
}

static int read(int fd, const void *buffer, unsigned size) {
    if (!validate_read_parameters(buffer, size)) {
        exit(-1);
    }

    uint8_t *user_dest = (uint8_t *)buffer;
    struct file *file_handle = get_file_by_fd(fd);
    int bytes_read = 0;

    if (fd != STDIN_FILENO) {
        bytes_read = perform_file_read(fd, file_handle, user_dest, size);
    } else {
        bytes_read = perform_stdin_read(user_dest, size);
    }

    return bytes_read;
}



/* Write system call. */
static bool validate_write_parameters(void *source, unsigned size) {
    return size != 0 && source != NULL && is_user_vaddr(source) && is_user_vaddr((void *)((char *)source + size));
}

static void perform_buffer_validation(const void *buffer, unsigned size) {
    for (unsigned i = 0; i < size; i++) {
        if (!is_user_vaddr((void *)((char *)buffer + i))) {
            exit(-1);
        }
    }
}

static int perform_file_write(struct file *file, uint8_t *source, size_t size, int fd) {
    int total_written = 0;
    off_t result;
    while (size > 0) {
        size_t page_offset = pg_ofs(source);
        size_t write_amount = size < PGSIZE - page_offset ? size : PGSIZE - page_offset;

        if (!page_lock(source, false))
            thread_exit();
        lock_acquire(&file_lock);
        result = file_write(file, source, write_amount);
        lock_release(&file_lock);
        page_unlock(source);

        if (result < 0) {
            return total_written > 0 ? total_written : -1;
        }
        total_written += result;
        if (result != (off_t) write_amount) {
            break; // Short write, so we're done.
        }

        source += result;
        size -= result;
    }
    return total_written;
}

static int perform_stdout_write(uint8_t *source, size_t size) {
    putbuf((char *)source, size);
    return size;
}

static int write(int handle, void *usrc_, unsigned size) {
    if (!validate_write_parameters(usrc_, size)) {
        exit(-1);
    }

    perform_buffer_validation(usrc_, size);

    uint8_t *user_source = (uint8_t *)usrc_;
    int bytes_written = 0;
    struct file *file_handle = NULL;

    if (handle != STDOUT_FILENO) {
        file_handle = get_file_by_fd(handle);
        if (file_handle == NULL) {
            exit(-1);
        }
    }

    if (handle == STDOUT_FILENO) {
        bytes_written = perform_stdout_write(user_source, size);
    } else {
        bytes_written = perform_file_write(file_handle, user_source, size, handle);
    }

    return bytes_written;
}


//Changes the next byte to be read or written in open file fd to position, 
//expressed in bytes from the beginning of the file.
void seek (int fd, unsigned position){
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
  struct file* f = get_file_by_fd(fd);
  //edge case handling if f is NULL
  if(f == NULL){return -1;}

  //synchronization lock due to potential changes in file
  lock_acquire(&file_lock);
  off_t pos = file_tell(f);//get pos via file_tell
  lock_release(&file_lock);

  return pos;
}

//Closes file descriptor fd. Exiting or terminating a process implicitly 
//closes all its open file descriptors, as if by calling this function for each one.
void close (int fd){
  //get the file by fd by using the helper function get_file_by_fd
  struct file* f = get_file_by_fd(fd);
  //edge case handling: f not found given fd, do nothing
  if(f!=NULL){
    lock_acquire(&file_lock);
    file_close(f);//closing the file if f exists
  
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
 if (!is_user_vaddr(f->esp)) {exit(-1);}

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
  //look at call_nr and match the sys number with the right sys call
  switch(call_nr){
    case SYS_HALT: 
      halt();
      break;
    case SYS_EXIT: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
      exit((int)args[0]);
      break;
    case SYS_EXEC: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
      f->eax = exec((const char*)args[0]);
      break;
    case SYS_WAIT: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
      f->eax = wait((pid_t)args[0]);
      break;
    case SYS_CREATE:
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 2);
      f->eax = create((const char*)args[0], (unsigned)args[1]);
      break;
    case SYS_REMOVE: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
      f->eax = remove((const char*)args[0]);
      break;
    case SYS_OPEN: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
      f->eax = open((const char*) args[0]);
      break;
    case SYS_FILESIZE:  
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
      f->eax = filesize((int)args[0]);
      break;
    case SYS_READ:  
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 3);
      f->eax = read((int)args[0], (const void*) args[1], (unsigned) args[2]);
      break;
    case SYS_WRITE:
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 3);
      f->eax = write((int)args[0], (const void*) args[1], (unsigned) args[2]);
      break;
    case SYS_SEEK: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 2);
      seek((int)args[0], (unsigned)args[1]);
      break;
    case SYS_TELL: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
      f->eax = tell((int)args[0]);
      break;
    case SYS_CLOSE: 
      copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 1);
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
  uint8_t *dst = dst_; 
  const uint8_t *usrc = usrc_;

  page_lock (usrc, false);
  //printf("after lock upage is %p\n", usrc);

  int i = 0;
  for (; size > 0; size--, dst++){
    if (usrc >= (uint8_t *) PHYS_BASE || !get_user (dst, usrc+i)){
      exit(-1);
    }
    i++;
  }
  

  page_unlock(usrc);
}

/* Creates a copy of user string US in kernel memory and returns it as a
   page that must be **freed with palloc_free_page()**.  Truncates the string
   at PGSIZE bytes in size.  Call thread_exit() if any of the user accesses
   are invalid. */
static char *
copy_in_string (const char *us)
{
  if (us == NULL || !is_user_vaddr(us)){exit(-1);}//check valid string


  char *ks = (char*)palloc_get_page (0);
  if (ks == NULL){exit(-1);}//failure on space allocation

  char* upage = pg_round_down(us);

  page_lock (upage, false);

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
      if((char)byte == '\0'){break;}
      //make sure ks is null-terminated if PGSIZE is reached
      if(i == PGSIZE){ks[PGSIZE-1] = '\0';}
    }

  page_unlock(upage);
  

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

