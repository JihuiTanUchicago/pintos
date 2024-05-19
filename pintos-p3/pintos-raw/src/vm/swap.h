#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <list.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/page.h"

/*
Just prototypes. See swap.c for more details.
*/

/* The swap device. */
static struct block* swap_device;

/* Used swap pages. */
static struct bitmap* swap_bitmap;

/* Protects swap_bitmap. */
static struct lock swap_lock;

/* Number of sectors per page. */
#define PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init (void);
void swap_in (struct page *p);
bool swap_out (struct page *p);
