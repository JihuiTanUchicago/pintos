#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <list.h>

struct frame{
    void* base;
    struct lock lock;
    struct page* page;
};

struct frame *frames;
size_t frame_cnt;

struct lock scan_lock;
size_t hand;


/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */

//static struct frame *try_frame_alloc_and_lock (struct page *page);
/* Tries really hard to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
struct frame *frame_alloc_and_lock (struct page *page);
/* Locks P's frame into memory, if it has one.
   Upon return, p->frame will not change until P is unlocked. */
void frame_lock (struct page *p);
/* Releases frame F for use by another page.
   F must be locked for use by the current process.
   Any data in F is lost. */
void frame_free (struct frame *f);
/* Unlocks frame F, allowing it to be evicted.
   F must be locked for use by the current process. */
void frame_unlock (struct frame *f);

#endif // FRAME_H