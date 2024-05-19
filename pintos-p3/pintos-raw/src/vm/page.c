#include "vm/page.h"
#include "userprog/syscall.h"
#include "threads/synch.h"

/* Destroys a page, which must be in the current process's
   page table.  Used as a callback for hash_destroy(). */
void destroy_page (struct hash_elem *p_, void *aux UNUSED)  {
   /* retrieve the page from hash_elem *p_ */
   struct page* page = hash_entry(p_, struct page, hash_elem);
   /* Make sure page is not accessed by others and safely free the page
   by using frame_lock() */
   frame_lock(page);
   if(page->frame != NULL){frame_free(page->frame);}
   free(page);
}

/* Destroys the current process's page table. */
void page_exit (void)  {
  if (thread_current ()->pages != NULL)
    hash_destroy(thread_current ()->pages, destroy_page);
}

/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
struct page *page_for_addr (const void *address) {
   //printf("in page.c, in page_for_addr.\n");
   if (address >= PHYS_BASE){
      //printf("visit here\n");
      return NULL;
   }else{
      struct page p;
      struct hash_elem *e;

      p.addr = (void*)pg_round_down(address);

      e = hash_find(thread_current()->pages, &p.hash_elem);

      // hash found now. 
      if (e!=NULL){
         //printf("in page.c, already mapped.\n");
         return hash_entry (e, struct page, hash_elem);
      } 
      
      //printf("in page.c, attempt to access stack.\n");
      //so it may cause a page fault 4 bytes below the stack pointer
      //so it can fault 32 bytes below the stack pointer
      void* above_is_stack = (void*)PHYS_BASE - STACK_MAX;
      
      void* saved_in_intterupt = thread_current()->user_stack_pointer;
      if (((saved_in_intterupt <= address+32) && (p.addr > above_is_stack)))
      {
         // page_allocate (void *vaddr, bool read_only)
         // false because stack can be written. 
         //printf("in page.c, tries to access stack.\n");
         //printf("in page.c, grow stack.\n");
         return page_allocate (p.addr, false);
      }
   }
   return NULL;
}

/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
static void load_data_from_swap(struct page *page) {
    swap_in(page);
}

static void load_data_from_file(struct page *page) {
    off_t read_size = file_read_at(page->exec_file, page->frame->base, page->bytes, page->offset);
    off_t zero_size = PGSIZE - read_size;
    memset(page->frame->base + read_size, 0, zero_size);
}

static void initialize_zero_page(struct page *page) {
    memset(page->frame->base, 0, PGSIZE);
}

static bool lock_and_load_page(struct page *page) {
    frame_lock(page);
    if (page->frame == NULL) {
        return do_page_in(page);
    }
    return true;
}

bool do_page_in(struct page *target_page) {
    target_page->frame = frame_alloc_and_lock(target_page);
    if (target_page->frame == NULL) return false;

    if (target_page->b_s != (block_sector_t)-1) {
        load_data_from_swap(target_page);
    } else if (target_page->exec_file != NULL) {
        load_data_from_file(target_page);
    } else {
        initialize_zero_page(target_page);
    }

    return true;
}


/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
bool page_in(void *fault_address) {
    struct page *fault_page = page_for_addr(fault_address);
    if (fault_page == NULL) return false;

    bool is_page_ready = lock_and_load_page(fault_page);
    ASSERT(is_page_ready && lock_held_by_current_thread(&fault_page->frame->lock));

    bool setup_successful = pagedir_set_page(thread_current()->pagedir, fault_page->addr, 
                                             fault_page->frame->base, !fault_page->read_only);

    frame_unlock(fault_page->frame);

    return setup_successful;
}


/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */
static void clear_page_directory_entry(struct page *p) {
    pagedir_clear_page(p->thread->pagedir, (void *)p->addr);
}

static bool write_page_to_disk(struct page *p, bool dirty) {
    if (p->exec_file == NULL) {
        return swap_out(p);
    } else if (dirty) {
        return p->swap_or_file ? swap_out(p) : file_write_at(p->exec_file, (const void *)p->frame->base, p->bytes, p->offset);
    }
    return true;
}

static bool swap_out_or_write_file(struct page *p) {
    bool dirty = pagedir_is_dirty(p->thread->pagedir, (const void *)p->addr);
    return write_page_to_disk(p, dirty);
}

static void reset_page_accessed_flag(struct page *p, bool was_accessed) {
    if (was_accessed) {
        pagedir_set_accessed(p->thread->pagedir, p->addr, false);
    }
}

bool page_out(struct page *p) {
    ASSERT(p->frame != NULL);
    ASSERT(lock_held_by_current_thread(&p->frame->lock));

    clear_page_directory_entry(p);

    bool ok = swap_out_or_write_file(p);

    if (ok) {
        p->frame = NULL;
    }
    return ok;
}


/* Returns true if page P's data has been accessed recently,
   false otherwise.
   P must have a frame locked into memory. */
bool page_accessed_recently(struct page *p) {
    ASSERT(p->frame != NULL);
    ASSERT(lock_held_by_current_thread(&p->frame->lock));

    bool accessed = pagedir_is_accessed(p->thread->pagedir, p->addr);
    reset_page_accessed_flag(p, accessed);

    return accessed;
}

/* Adds a mapping for user virtual address VADDR to the page hash
   table. Fails if VADDR is already mapped or if memory
   allocation fails. */
static void initialize_page(struct page *p, void *vaddr, bool read_only) {
    p->frame = NULL;
    p->addr = pg_round_down(vaddr);
    p->read_only = read_only;
    p->swap_or_file = !read_only;
    p->thread = thread_current();
    p->b_s = (block_sector_t)-1;
    p->exec_file = NULL;
    p->offset = 0;
    p->bytes = 0;
}

static bool insert_page_in_hash(struct page *p) {
    return hash_insert(p->thread->pages, &p->hash_elem) == NULL;
}

static void evict_page(struct page *p) {
    if (p->exec_file && !p->swap_or_file)
        page_out(p);
    frame_free(p->frame);
}

struct page *page_allocate(void *vaddr, bool read_only) {
    struct page *page = malloc(sizeof(struct page));
    if (!page) {
        return NULL;
    }

    initialize_page(page, vaddr, read_only);

    if (!insert_page_in_hash(page)) {
        free(page);
        return NULL;
    }
    return page;
}


/* Evicts the page containing address VADDR
   and removes it from the page table. */
void page_deallocate(void *vaddr) {
    struct page *p = page_for_addr(vaddr);
    ASSERT(p);
    frame_lock(p);

    if (p->frame) {
        evict_page(p);
    }
    hash_delete(thread_current()->pages, &p->hash_elem);
    free(p);
}


/* Returns a hash value for the page that E refers to. */
unsigned page_hash (const struct hash_elem *e, void *aux UNUSED) {
   const struct page *p = hash_entry (e, struct page, hash_elem);
   return hash_bytes(&p->addr, sizeof p->addr);
}

/* Returns true if page A precedes page B. */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
   const struct page *a = hash_entry (a_, struct page, hash_elem);
   const struct page *b = hash_entry (b_, struct page, hash_elem);
   return a->addr < b->addr;
}

/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
static bool load_page(struct page *p) {
    return do_page_in(p) && pagedir_set_page(thread_current()->pagedir, p->addr, p->frame->base, !p->read_only);
}

bool page_lock(const void *addr, bool will_write) {
    struct page *p = page_for_addr(addr);
    if (!p || (p->read_only && will_write)) {
        return false;
    }
    frame_lock(p);
    if (p->frame) {
        return true;
    }
    return load_page(p);
}


/* Unlocks a page locked with page_lock(). */
void page_unlock (const void *addr){
  struct page *p = page_for_addr (pg_round_down(addr));
  ASSERT (p);
  frame_unlock (p->frame);
}
