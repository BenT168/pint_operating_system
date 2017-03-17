#include "vm/swap.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include <bitmap.h>
#include <hash.h>
#include <debug.h>
#include <stdio.h>

#define PGSIZE 4096                                 /* The size of a page. */
#define NBR_BLOCKS (PGSIZE / BLOCK_SECTOR_SIZE)     /* The number of sectors in a page. */

static struct block *swap_space;                    /* Block storing swapped table */
static struct lock swap_lock;                       /* Global lock to stop concurrent access of the swap table. */
static struct bitmap * swap_bitmap;                 /* The Swap Table Bitmap */

unsigned swap_size;                                  /* Size of the Swap Block  */

static void acquire_swaplock (void);
static void release_swaplock (void);
block_sector_t swap_get_free(void);

/* TASK 3 : Acquires lock over swap table */
void
acquire_swaplock (void)
{
  lock_acquire (&swap_lock);
}

/* TASK 3 : Releases lock from swap table */
void
release_swaplock (void)
{
  lock_release (&swap_lock);
}

/* TASK 3: initialises swap table and values */
void
swap_init ()
{
  /* block device used for swapping */
  swap_space = block_get_role (BLOCK_SWAP);

  /* get the size of the block */
  swap_size = block_size (swap_space);

  /* initializes the swap lock */
  lock_init(&swap_lock);

  swap_bitmap = bitmap_create (swap_size);

  acquire_swaplock();
	bitmap_set_all(swap_bitmap, 0);
	release_swaplock();
}

/* TASK 3: Construct the swap slot and dereference to frame */
struct
swap_slot * swap_slot_construct (struct frame * frame)
{
  struct swap_slot * swap_slot = malloc (sizeof (struct swap_slot));
  swap_slot->swap_frame = frame;
  return swap_slot;
}

/* TASK 3: Checking as if the swap table is full or else flips them all and
   returns the index of the first bit in the group. */
block_sector_t swap_get_free ()
{
  bool isFull = bitmap_all (swap_bitmap, 0 , swap_size);
  if (isFull)
  {
    PANIC("SWAP id full! Memory exhausted!");
  }
  return bitmap_scan_and_flip (swap_bitmap, 0, NBR_BLOCKS, NULL);
}

/* TASK 3 : Load the swapping address by reading the block device */
void
swap_load (void *upageaddr, struct swap_slot* ss)
{
  acquire_swaplock();
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_read (swap_space, ss->swap_addr + i, upageaddr + i * BLOCK_SECTOR_SIZE);
  bitmap_set_multiple (swap_bitmap, ss->swap_addr, NBR_BLOCKS, NULL);
  release_swaplock();
}


/* TASK 3 : Store the swapping address by writing into the block device */
size_t
swap_store (void *vaddr)
{
  acquire_swaplock();
  block_sector_t swap_addr = swap_get_free();
  for (int i = 0; i < NBR_BLOCKS; i++)
    block_write (swap_space, swap_addr + i, vaddr + i *BLOCK_SECTOR_SIZE);
  release_swaplock();
  return (size_t) swap_addr;
}

/* TASK 3: Free swap slots */
void
swap_free (struct swap_slot* ss)
{
  acquire_swaplock();
  bitmap_set_multiple (swap_bitmap, ss->swap_addr, NBR_BLOCKS, false);
  release_swaplock();
}
