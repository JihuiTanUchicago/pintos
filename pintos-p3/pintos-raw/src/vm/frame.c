#include "vm/frame.h"
#include "threads/synch.h"

/*
Managing the frame table

The main job is to obtain a free frame to map a page to. To do so:

1. Easy situation is there is a free frame in frame table and it can be
obtained. If there is no free frame, you need to choose a frame to evict
using your page replacement algorithm based on setting accessed and dirty
bits for each page. See section 4.1.5.1 and A.7.3 to know details of
replacement algorithm(accessed and dirty bits) If no frame can be evicted
without allocating a swap slot and swap is full, you should panic the
kernel.

2. remove references from any page table that refers to.

3. write the page to file system or swap.

*/

// we just provide frame_init() for swap.c
// the rest is your responsibility
int MAX_FRAME_ALLOC_ATTEMPTS = 5;

void
frame_init (void)
{
  hand = 0;
  void* base;
  //lock initialized here. 
  lock_init (&scan_lock);
  // frames seems to be a list of frames. 
  frames = malloc (sizeof *frames * init_ram_pages);
  if (frames == NULL){
      PANIC ("out of memory allocating page frames");
  }

  while ((base = palloc_get_page (PAL_USER)) != NULL)
    {
      struct frame *f = &frames[frame_cnt++];
      lock_init (&f->lock);
      f->base = base;
      f->page = NULL;
    }   
}

static struct frame *assign_page_to_frame(struct frame *frame, struct page *page) {
    frame->page = page;
    return frame;
}

/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
static struct frame *find_free_frame(struct page *target_page) {
    for (size_t index = 0; index < frame_cnt; index++) {
        struct frame *current_frame = &frames[index];
        if (!lock_try_acquire(&current_frame->lock)) continue;
        
        if (current_frame->page == NULL) {
            return assign_page_to_frame(current_frame, target_page);
        }
        lock_release(&current_frame->lock);
    }
    return NULL;
}

static struct frame *find_frame_to_evict(struct page *target_page) {
    for (size_t index = 0; index < frame_cnt * 2; index++) {
        size_t current_index = (hand + index) % frame_cnt;
        struct frame *current_frame = &frames[current_index];

        if (!lock_try_acquire(&current_frame->lock)) continue;

        if (current_frame->page == NULL) {
            return assign_page_to_frame(current_frame, target_page);
        }

        if (page_accessed_recently(current_frame->page)) {
            lock_release(&current_frame->lock);
            continue;
        }

        if (!page_out(current_frame->page)) {
            lock_release(&current_frame->lock);
            return NULL;
        }

        return assign_page_to_frame(current_frame, target_page);
    }
    return NULL;
}

struct frame *frame_alloc_and_lock(struct page *page) {
    lock_acquire(&scan_lock);

    struct frame *free_frame = find_free_frame(page);
    if (free_frame != NULL) {
        lock_release(&scan_lock);
        return free_frame;
    }

    struct frame *frame_to_evict = find_frame_to_evict(page);
    lock_release(&scan_lock);
    return frame_to_evict;
}


static void acquire_frame_lock(struct frame *frame) {
    lock_acquire(&frame->lock);
}

static bool verify_frame_consistency(struct page *page, struct frame *original_frame) {
    if (original_frame != page->frame) {
        lock_release(&original_frame->lock);
        ASSERT(page->frame == NULL);
        return false;
    }
    return true;
}


/* Locks P's frame into memory, if it has one.
   Upon return, p->frame will not change until P is unlocked. */
void frame_lock(struct page *page) {
    struct frame *current_frame = page->frame;
    if (current_frame != NULL) {
        acquire_frame_lock(current_frame);
        if (!verify_frame_consistency(page, current_frame)) {
            return;
        }
    }
}


/* Unlocks frame F, allowing it to be evicted.
   F must be locked for use by the current process. */
void frame_unlock(struct frame *frame) {
    ASSERT(lock_held_by_current_thread(&frame->lock));
    lock_release(&frame->lock);
}


/* Releases frame F for use by another page.
   F must be locked for use by the current process.
   Any data in F is lost. */
void frame_free(struct frame *frame) {
    ASSERT(lock_held_by_current_thread(&frame->lock));

    frame->page = NULL;
    lock_release(&frame->lock);
}

