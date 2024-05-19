#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

//define maximum arguments program could accept
#define MAX_ARGS 50

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
char* return_file_name_only(const char* command);


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t process_execute(const char *file_name){
  //edge case handling: if file_name is not a valid pointer, return TID_ERROR
  if(file_name == NULL){
    return TID_ERROR;
  }

  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = malloc(strlen(file_name)+1);
  if (fn_copy == NULL){
    return TID_ERROR;
  }
  strlcpy(fn_copy, file_name, strlen(file_name)+1);
  //The file_name here also needs to change
  //parse out the file_name_only here.
  char* file_name_only = return_file_name_only(file_name);

  //edge case handling: if file_name_only is NULL, return with TID_ERROR
  if (file_name_only==NULL){
    free(fn_copy);
    return TID_ERROR;
  }

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name_only, PRI_DEFAULT, start_process, fn_copy);
  
  /*sema down to prevent race condition with start_process
  wake up until tid is returned*/
  free(file_name_only);

  sema_down(&thread_current()->exit_child_sema_arr[thread_current()->current_child_tid]);
  
  //edge case handling: thread create failed
  if (tid == TID_ERROR || thread_current()->exit_child_code_arr[tid] == -1){
    free(fn_copy); 
    return TID_ERROR;
  }

  //set exit_child_tid to tid
  thread_current()->exit_child_tid_arr[tid] = tid;


  free(fn_copy); 
  return tid;
}

// returns the first argument of a command. 
char* return_file_name_only(const char* cmdline){
  //base case: check valid pointer and if cmdline is empty
  if(cmdline == NULL || *cmdline == '\0'){
    return NULL;
  }

  //get first arguments using strtok_r, but let's get a copy first
  //in case cmdline could be modified
  char* temp_cmdline = malloc(strlen(cmdline)+1);
  if(temp_cmdline == NULL || *temp_cmdline == '\0'){
    return NULL;
  }
  strlcpy(temp_cmdline, cmdline, strlen(cmdline)+1);

  char* saveptr; 
  char* token = strtok_r(temp_cmdline, " ", &saveptr); 
  //edge case handling: if first_arg is NULL
  if(token == NULL) {
    free(temp_cmdline);
    return NULL;
  }

  //getting first_arg from token
  char* first_arg = malloc(strlen(token) + 1);
  //edge case handling: if malloc failed
  if(first_arg == NULL){
    return NULL;
  }
  strlcpy(first_arg, token, strlen(token) + 1);


  free(temp_cmdline);
  return first_arg;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_){
  struct thread* cur = thread_current();
  //the file_name right now is a whole command line, so I only want the first one
  //use string tokenizer to put the file name there. 
  char *file_name_only = return_file_name_only(file_name_);
  if (file_name_only == NULL || *file_name_only == '\0'){ //edge case handling: somehow file_name is NULL or empty
    sema_up(&cur->parent->exit_child_sema_arr[cur->tid]);
    exit(-1);
  }else{
    struct intr_frame if_;
    bool success;
    /* Initialize interrupt frame and load executable. */
    memset (&if_, 0, sizeof if_);
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;

    //load takes the whole command line
    success = load (file_name_, &if_.eip, &if_.esp);

    /*sema up to signal parent to wake up not matter what*/
    sema_up(&cur->parent->exit_child_sema_arr[cur->tid]);

    /* If load failed, quit. */
    if (!success){
      exit(-1);
    }else{
      /* Start the user process by simulating a return from an
        interrupt, implemented by intr_exit (in
        threads/intr-stubs.S).  Because intr_exit takes all of its
        arguments on the stack in the form of a `struct intr_frame',
        we just point the stack pointer (%esp) to our stack frame
        and jump to it. */
      asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
      NOT_REACHED ();
    }
  }
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int process_wait (tid_t child_tid UNUSED) 
{
  struct thread *parent = thread_current();

  //check if child_tid is valid
  if(child_tid < 0 || child_tid >= MAX_CHILDREN ||
     parent->exit_child_tid_arr[child_tid] != child_tid || parent->exit_child_waited != 0){
    return -1;
  }


  //mark that child has been waited;
  parent->exit_child_waited = 1;

  //sema_down if child has not exited
  sema_down(&parent->exit_child_sema_arr[child_tid]);
  //parent now should be woken up and
  int exit_child_code = parent->exit_child_code_arr[child_tid];
  parent->exit_child_tid_arr[child_tid] = UNINITIALIZED; //important for wait-twice
  parent->exit_child_waited = 0;

  return exit_child_code;
}

/* Free the current process's resources. */
void process_exit (void){
  //printf("(%s)process_exit: entered process_exit \n", thread_current()->name);
  struct thread *cur = thread_current ();

  //pass information to its parent to signal its exit before child gone
  if(cur->parent != NULL){
    cur->parent->exit_child_tid_arr[cur->tid] = cur->tid;
    cur->parent->exit_child_code_arr[cur->tid] = cur->exit_code;   
    //sema_up here to signal parent's wait, if any
    sema_up(&cur->parent->exit_child_sema_arr[cur->tid]);
  }

  uint32_t *pd;
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
   file_close(cur->executable);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void){
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in //printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char* command_line);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  //take out only the file name
  char* file_name_only = return_file_name_only(file_name);
  t->executable = file = filesys_open (file_name_only);

  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name_only);
      goto done; 
    }

  //passed rox-simple
  file_deny_write (file); 

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name_only);
      
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                { 
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  //file_name is actually the whole command line
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  // file_close (file);
  //printf("(%s)load: load complete(don't know success or not). \n", thread_current()->name);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}


static bool
setup_stack (void **esp, const char* command_line) 
{

  uint8_t *kpage;
  bool success = false;


  //size of command line string is not larger than PGSIZE. and if it is larger 
  //you should return false.
  if (strlen(command_line)>=PGSIZE){
    return false;
  }
  

  //list_of_args actually saves a list of pointers. I assume at most 50 arguments. 
  //when free, will need a for loop. 
  //also check if malloc is successful or not
  char **list_of_args;
  list_of_args = malloc(MAX_ARGS * sizeof(char*));
  if(list_of_args == NULL){
    return false;
  }

  // count how many arguments there are so that I can malloc the list. 
  char* token;
  char* rest = command_line;
  int num_of_args = 0; //num_of_args canont exceed MAX_ARGS
  while ((token = strtok_r(rest, " ", &rest)) && num_of_args < MAX_ARGS){
    int tok_length = strlen(token);
    list_of_args[num_of_args] = malloc((tok_length+1) * sizeof(char));
    if(list_of_args[num_of_args] == NULL){
      free(list_of_args);
      return false;
    }
    strlcpy(list_of_args[num_of_args], token, (tok_length+1));
    num_of_args++;
  }

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
 
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success){
        *esp = PHYS_BASE;

        int total_size = 0;
        //must also record a list of addresses.a list
        uint32_t *pointerList[num_of_args];

        //from end of the list to the front of the list
        for(int i = (num_of_args-1); i>-1; i--){
          //below I copy in the arguments onto the stack. 
          char* cur = list_of_args[i];
          int cur_size = strlen(cur)+1; //plus in \0
          *esp -= cur_size*sizeof(char); //minus down then copy. 
          total_size += cur_size;
          memcpy (*esp, cur, sizeof(char)*cur_size);
          //modify outer variables
          pointerList[i] = *esp;
        }
        //padding as well as for the word alignment needed. 
        int remainder = (4 - total_size%4)+4;
 
        while(remainder != 0){
          *esp -= 1*sizeof(char);
          memcpy (*esp, "\0", sizeof(char)*1);
          remainder--;
        }
        
        for(int i = (num_of_args-1); i>-1; i--){
          char* cur = (char*)pointerList[i];
          *esp -= 4*sizeof(char);
          memcpy (*esp, &cur, sizeof(char)*4);
        }
        //above finished the pointers
        //now I should have the addresses. 
        
        //push argv
        char** cur = *esp;
        *esp -= sizeof(char**);
        memcpy(*esp, &cur, sizeof(char**));

        //push argc
        *esp -= sizeof(int);
        memcpy(*esp, &num_of_args, sizeof(int));

        //push fake return address 
        *esp -= sizeof(void*);
        memcpy(*esp, &(void*){NULL}, sizeof(void*));
      }
      else{
        palloc_free_page (kpage); 
      }
        
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  //edge case error handling if kapge is NULL
  if(kpage == NULL){
    return false;
  }
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
