#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <list.h>
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "vm/frame.h"

#include "devices/block.h"
/*
Just prototypes, see page.c for more detail
*/

#define STACK_MAX (1024 * 1024)

struct page{
    struct frame* frame;
    struct hash_elem hash_elem; /* Hash table element. */ 
    void *addr; /* Virtual address. */
    struct thread* thread; /* owner thread of the page */
    bool read_only;
    bool swap_or_file;

    struct file* exec_file;
    off_t offset;
    off_t bytes;

    block_sector_t b_s;
};

void destroy_page (struct hash_elem *p_, void *aux );
void page_exit (void);
struct page *page_for_addr (const void *address);
bool do_page_in (struct page *p);
bool page_in (void *fault_addr);
bool page_out (struct page *p);
bool page_accessed_recently (struct page *p);
struct page * page_allocate (void *vaddr, bool read_only);
void page_deallocate (void *vaddr);
unsigned page_hash (const struct hash_elem *e, void *aux );
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux );
bool page_lock (const void *addr, bool will_write);
void page_unlock (const void *addr);

#endif // PAEG_H