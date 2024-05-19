#include "vm/swap.h"
/*

Managing the swap table

You should handle picking an unused swap slot for evicting a page from its
frame to the swap partition. And handle freeing a swap slot which its page
is read back.

You can use the BLOCK_SWAP block device for swapping, obtaining the struct
block that represents it by calling block_get_role(). Also to attach a swap
disk, please see the documentation.

and to attach a swap disk for a single run, use this option ‘--swap-size=n’

*/

/* Set up*/
void
swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device == NULL)
    {
      printf ("no swap device--swap disabled\n");
      swap_bitmap = bitmap_create (0);
    }
  else
    swap_bitmap = bitmap_create (block_size (swap_device)
                                 / PAGE_SECTORS);
  if (swap_bitmap == NULL)
    PANIC ("couldn't create swap bitmap");
  lock_init (&swap_lock);
}

/* Swaps in page P, which must have a locked frame
   (and be swapped out). */
static void read_from_swap(struct page *p, block_sector_t sector_offset) {
    for (size_t sector = 0; sector < PAGE_SECTORS; ++sector) {
        void *target_addr = (uint8_t *)p->frame->base + sector * BLOCK_SECTOR_SIZE;
        block_read(swap_device, sector_offset + sector, target_addr);
    }
}

static void write_to_swap(struct page *p, block_sector_t sector_offset) {
    for (size_t sector = 0; sector < PAGE_SECTORS; ++sector) {
        void *source_addr = (uint8_t *)p->frame->base + sector * BLOCK_SECTOR_SIZE;
        block_write(swap_device, sector_offset + sector, source_addr);
    }
}

static void reset_swap_slot(block_sector_t sector_offset) {
    size_t swap_index = sector_offset / PAGE_SECTORS;
    bitmap_reset(swap_bitmap, swap_index);
}

/* Swaps in page P, which must have a locked frame
   (and be swapped out). */
void swap_in(struct page *p) {
    ASSERT(p && p->frame);
    ASSERT(lock_held_by_current_thread(&p->frame->lock));
    ASSERT(p->b_s != (block_sector_t) -1);

    read_from_swap(p, p->b_s);
    reset_swap_slot(p->b_s);
    p->b_s = (block_sector_t) -1;
}


/* Swaps out page P, which must have a locked frame. */
static void reset_page_file_info(struct page *p) {
    p->exec_file = NULL;
    p->offset = 0;
    p->bytes = 0;
    p->swap_or_file = false;
}

bool swap_out(struct page *target_page) {
    ASSERT(target_page != NULL && target_page->frame != NULL);
    ASSERT(lock_held_by_current_thread(&target_page->frame->lock));

    size_t swap_index;
    lock_acquire(&swap_lock);
    swap_index = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    lock_release(&swap_lock);

    if (swap_index == BITMAP_ERROR) {
        return false;
    }

    block_sector_t swap_sector = swap_index * PAGE_SECTORS;
    target_page->b_s = swap_sector;

    for (size_t i = 0; i < PAGE_SECTORS; i++) {
        block_write(swap_device, swap_sector + i, (uint8_t *)target_page->frame->base + i * BLOCK_SECTOR_SIZE);
    }

    reset_page_file_info(target_page);

    return true;
}


