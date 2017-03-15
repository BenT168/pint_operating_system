#ifndef VM_PTUTILS_H
#define VM_PTUTILS_H

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* There are similar definitions in "threads/vaddr.h". However, we redefine
   them here with an prefix 'S_' in order to decouple the implementation of the
   supplementary page table from the provided PintOS page directory/page table
   structure. */
#define S_PTE_FLAGS 0x00000fff    /* Flag bits. */
#define S_PTE_ADDR  0xfffff000    /* Address bits. */
#define S_PTE_AVL   0x00000e00    /* Bits available for OS use. */
#define S_PTE_P 0x8               /* 1=present, 0=not present. */
#define S_PTE_W 0x4               /* 1=read/write, 0=read-only. */
#define S_PTE_R 0x2               /* 1=referenced, 0=not referenced. */
#define S_PTE_M 0x1               /* 1=modified (dirty),
                                     0=not modified (not dirty). */

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. */
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). */
#define MAX_PTE_FLAGS 4

uint32_t convert_flags (pte_flags flags[], size_t arr_len);
pte_flags[]* extract_flags (uint32_t flags_dword);
int hamming_weight (uint32_t i);
