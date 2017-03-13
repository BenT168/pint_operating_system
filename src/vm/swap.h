#ifndef _VM_SWAP_H
#define _VM_SWAP_H

#include "vm/frame.h"
#include "devices/block.h"

struct swap_slot {
	struct frame *swap_frame;
	block_sector_t swap_addr;	 /* Address of the first segment where the page stored */
};

void swap_init (void);
void swap_load (void* , struct swap_slot* ss);
void swap_store (struct swap_slot* ss);
void swap_free (struct swap_slot* ss);
struct swap_slot* swap_slot_construct(struct frame* frame);

#endif
