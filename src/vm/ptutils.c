#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "vm/page.h"
#include "vm/ptutils.h"

/* Calculates the hamming weight of unsigned integer 'i'. */
int
hamming_weight (uint32_t i)
{
  // Magic
  i = i - ((i >> 1) & 0x55555555);
  i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
  return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) * 24;
}
