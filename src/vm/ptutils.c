#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "vm/page.h"
#include "vm/ptutils.h"

/* @DEPRECATED */

/* Converts flags given in 'flags' array to integer form: stores them as
   set/unset bits in 'flags_dword'. This function is used only for page table
   entry flags and is dependent on the definitions in the enum 'ple_flags'. */
uint32_t
convert_flags (pte_flags flags[], size_t arr_len)
{
  uint32_t flags_dword = 0;

  for (size_t i = 0; i < arr_len; i++) {
    flags[i] & flags_dword
  }
  return flags_dword;
}

pte_flags[]*
extract_flags (uint32_t flags_dword)
{
  enum pte_flags (*flags)[] = malloc (sizeof (pte_flags[MAX_PTE_FLAGS]));

  if (flags_dword & S_PTE_P)
    flags[0] = PRESENT;
  if (flags_dword & S_PTE_W)
    flags[1] = READWRITE;
  if (flags_dword & S_PTE_R)
    flags[2] = REFERENCED;
  if (flags_dword & S_PTE_M)
    flags[3] = MODIFIED;
  return flags;
}

void

/* Calculates the hamming weight of unsigned integer 'i'. */
int
hamming_weight (uint32_t i)
{
  // Magic
  i = i - ((i >> 1) & 0x55555555);
  i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
  return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) * 24;
}
