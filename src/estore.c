/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: January 2023 */

/* Store Management Routines. Nowadays the more common term is "memory". This
elaborate private block management scheme was invented in the days of RISC OS,
when processors were slower and malloc/free carried a significant cost. I
wonder whether it provides any performance benefit nowadays? */

#include "ehdr.h"

/* Debugging flags that can be set */

/* #define sanity */
/* #define TraceStore */
/* #define FullTraceStore */

/* Memory is taken from the system in blocks whose minimum size is
parameterized here. This block size must be a multiple of sizeof(freeblock),
which is typically 8 bytes in 32-bit systems or 16 in 64-bit systems, or
possibly 12 if size_t is only 4?. Set a value suitable for all these cases. */

#define store_allocation_unit    48*1024L

/* Free queue entries start with a pointer to the next entry followed by the
length. */

typedef struct {
  void  *free_block_next;
  size_t free_block_length;
} freeblock;

/* Blocks that are passed out and returned have their length at the start; the
address given/received from callers is that of the byte after the length. */

typedef struct {
  size_t block_length;
} block;



/*************************************************
*                  Static data                   *
*************************************************/

static freeblock *store_anchor;
static freeblock *store_freequeue;

#ifdef TraceStore
static size_t storetotal = 0;
#endif



/*************************************************
*         Free queue sanity check                *
*************************************************/

#ifdef sanity
/* Free queue sanity check */

void
store_freequeuecheck(void)
{
freeblock *p = store_freequeue->free_block_next;
while (p != NULL)
  {
  freeblock *next = (freeblock *)(((uschar *)p) + p->free_block_length);
  if (p->free_block_next != NULL && next > p->free_block_next)
    {
    debug_printf("OVERLAP %d %d %d %d\n", p, p->free_block_length, next,
      p->free_block_next);
    }
  p = p->free_block_next;
  }
}
#endif



/*************************************************
*                Initialize                      *
*************************************************/

/* A dummy first entry on the free queue is created. This is used solely as a
means of anchoring the queue. Searches always start at the block it points to.
*/

void
store_init(void)
{
store_anchor = NULL;
store_freequeue = (freeblock *)malloc(sizeof(freeblock));
store_freequeue->free_block_next = NULL;
store_freequeue->free_block_length = sizeof(freeblock);
}



/*************************************************
*               Free all store                   *
*************************************************/

/* Blocks of store obtained from the OS are chained together through their
first word(s). This function is called when NE is ending. */

void
store_free_all(void)
{
while (store_anchor != NULL)
  {
  freeblock *p = store_anchor;
  store_anchor = store_anchor->free_block_next;
  free(p);
  }
free(store_freequeue);
}



/*************************************************
*               Get block                        *
*************************************************/

void *
store_get(size_t bytesize)
{
int blockrem;
size_t newlength;
size_t truebytesize;
freeblock *newblock;
freeblock *previous = store_freequeue;
freeblock *p = previous->free_block_next;

#ifdef FullTraceStore
freeblock *pdebug = previous->free_block_next;
#endif

/* Add space for an initial block to hold the length, and ensure that the size
is a multiple of sizeof(freeblock) so that blocks can be amalgamated when
freed. */

truebytesize = bytesize + sizeof(block);
blockrem = truebytesize % sizeof(freeblock);
if (blockrem != 0) truebytesize += sizeof(freeblock) - blockrem;

#ifdef sanity
store_freequeuecheck();
#endif

/* Keep statistics */

#ifdef TraceStore
main_storetotal += truebytesize;
#endif

#ifdef FullTraceStore
while (pdebug != NULL)
  {
  debug_printf("G1    %8p %8p %8ld\n", pdebug, pdebug->free_block_next, pdebug->free_block_length);
  pdebug = pdebug->free_block_next;
  }
#endif

/* Search free queue for a block that is big enough */

while(p != NULL)
  {
  if (truebytesize <= p->free_block_length)
    {    /* found suitable block */
    block *pp = (block *)p;
    size_t leftover = p->free_block_length - truebytesize;
    if (leftover == 0)
      {  /* block used completely */
      previous->free_block_next = p->free_block_next;
      }
    else
      {  /* use bottom of block */
      freeblock *remains = (freeblock *)(((uschar *)p) + truebytesize);
      remains->free_block_length = leftover;
      remains->free_block_next = p->free_block_next;
      previous->free_block_next = remains;
      }

    #ifdef sanity
    store_freequeuecheck();
    #endif

    #ifdef TraceStore
    debug_printf("Get  %5ld %8ld %8p %8ld\n", truebytesize, main_storetotal,
      (void *)pp, leftover);
    #endif

    pp->block_length = truebytesize;
    #ifdef FullTraceStore
    pdebug = store_freequeue->free_block_next;
    while (pdebug != NULL)
      {
      debug_printf("G2    %8p %8p %8ld\n", pdebug, pdebug->free_block_next,
        pdebug->free_block_length);
      pdebug = pdebug->free_block_next;
      }
    #endif
    return (void *)(pp + 1);    /* leave length block hidden */
    }
  else
    {    /* try next block */
    previous = p;
    p = p->free_block_next;
    }
  }

/* No block long enough has been found */

#ifdef TraceStore
main_storetotal -= truebytesize;  /* correction */
#endif

newlength = (truebytesize + sizeof(freeblock) > (int)store_allocation_unit)?
  truebytesize + sizeof(freeblock) : (int)store_allocation_unit;
newblock = (freeblock *)malloc(newlength);

/* If malloc() fails, return NULL */

if (newblock == NULL) return NULL;

/* malloc() succeeded */

else
  {
  block *newbigblock = (block *)(newblock + 1);

  newblock->free_block_next = store_anchor;      /* Chain blocks through their */
  store_anchor = newblock;                       /* first block */
  newlength -= sizeof(freeblock);

  newbigblock->block_length = newlength;         /* Set block length */
#ifdef TraceStore
  main_storetotal += newlength;                  /* Correction */
#endif
  store_free(newbigblock + 1);                   /* Add to free queue */
  return store_get(bytesize);                    /* Try again */
  }
}



/*************************************************
*     Get store, failing if none available       *
*************************************************/

void *
store_Xget(size_t bytesize)
{
void *yield = store_get(bytesize);
if (yield == NULL) error_moan(1, bytesize);  /* Hard */
return yield;
}



/*************************************************
*          Get a line buffer                     *
*************************************************/

void *
store_getlbuff(size_t size)
{
linestr *line = store_Xget(sizeof(linestr));
uschar *text = (size == 0)? NULL : store_Xget(size);
line->prev = line->next = NULL;
line->text = text;
line->key = line->flags = 0;
line->len = size;
return line;
}



/*************************************************
*                 Copy store                     *
*************************************************/

void *
store_copy(void *p)
{
if (p == NULL) return NULL; else
  {
  size_t length = ((block *)p-1)->block_length - sizeof(block);
  void *yield = store_Xget(length);
  memcpy(yield, p, length);
  return yield;
  }
}



/*************************************************
*                 Copy a line                    *
*************************************************/

linestr *
store_copyline(linestr *line)
{
linestr *yield = store_getlbuff(line->len);
uschar *yieldtext = yield->text;
memcpy((void *)yield, (void *)line, sizeof(linestr));
yield->prev = yield->next = NULL;
yield->text = yieldtext;
if (line->len > 0) memcpy(yield->text, line->text, line->len);
return yield;
}



/*************************************************
*                   Copy string                  *
*************************************************/

uschar *
store_copystring(uschar *s)
{
if (s == NULL) return NULL; else
  {
  uschar *yield = store_Xget(Ustrlen(s)+1);
  Ustrcpy(yield, s);
  return yield;
  }
}



/*************************************************
*               Copy start/end string            *
*************************************************/

uschar *
store_copystring2(uschar *f, uschar *t)
{
int n = t - f;
uschar *yield = store_Xget(n + 1);
memcpy(yield, f, (unsigned)n);
yield[(unsigned)n] = 0;
return yield;
}



/*************************************************
*           Free a chunk of store                *
*************************************************/

/* The length is in the first word of the block, which is before the address
that the client was given. If the argument is NULL, do nothing (used for the
contents of empty lines). */

void
store_free(void *address)
{
size_t length;
freeblock *previous, *this, *start, *end;
#ifdef FullTraceStore
freeblock *pdebug = store_freequeue->free_block_next;
#endif

if (address == NULL) return;
#ifdef FullTraceStore
while (pdebug != NULL)
{ debug_printf("F1    %8p %8p %8ld\n", pdebug, pdebug->free_block_next, pdebug->free_block_length);
  pdebug = pdebug->free_block_next;
}
#endif

previous = store_freequeue;
this = previous->free_block_next;

start = (freeblock *) (((block *)address) - 1);
length = ((block *)start)->block_length;
end = (freeblock *)((uschar *)start + length);

#ifdef sanity
store_freequeuecheck();
#endif

#ifdef TraceStore
main_storetotal -= length;
debug_printf("Free %5ld %8ld %8p\n", length, main_storetotal, (void *)start);
#endif

/* Find where to insert */

while (this != NULL)
  {
  if (start < this) break;
  previous = this;
  this = previous->free_block_next;
  }

/* Insert */

previous->free_block_next = start;
start->free_block_next = this;
start->free_block_length = length;

/* Check for overlap with next */

if (end > this && this != NULL)
  {
  #ifdef TraceStore
  debug_printf("Upwards overlap: start=%p length=%5ld next=%p\n",
    (void *)start, length, (void *)this);
  #endif
  error_moan(2, start, length, this);  /* LCOV_EXCL_LINE */
  }

/* Check for contiguity with next */

if (end == this)
  {
  start->free_block_next = this->free_block_next;
  start->free_block_length += this->free_block_length;
  }

/* Check for overlap/contiguity with previous */

if (previous != store_freequeue)
  {
  freeblock *prevend = (freeblock *)(((uschar *)previous) + previous->free_block_length);
  if (prevend > start)
    {
    #ifdef TraceStore
    debug_printf("Downwards overlap: start=%p length=%5lu next=%p\n",
      (void *)previous, length, (void *)start);
    #endif
    error_moan(3, previous, previous->free_block_length, start); /* LCOV_EXCL_LINE */
    }

  if (prevend == start)
    {
    previous->free_block_next = start->free_block_next;
    previous->free_block_length += start->free_block_length;
    }
  }

#ifdef sanity
store_freequeuecheck();
#endif
#ifdef FullTraceStore
pdebug = store_freequeue->free_block_next;
while (pdebug != NULL)
{ debug_printf("F2    %8p %8p %8ld\n", pdebug, pdebug->free_block_next, pdebug->free_block_length);
  pdebug = pdebug->free_block_next;
}
#endif
}



/*************************************************
*       Reduce the length of a chunk of store    *
*************************************************/

void
store_chop(void *address, size_t bytesize)
{
usint blockrem;
usint freelength;
block *start, *end;

start = ((block *)address) - 1;

/* Round up new length as for new blocks */

bytesize += sizeof(block);
blockrem = bytesize % sizeof(freeblock);
if (blockrem != 0) bytesize += sizeof(freeblock) - blockrem;

#ifdef TraceStore
debug_printf("Chop %5lu %p\n", bytesize, (void *)start);
#endif

/* Compute amount to free, and don't bother if less than four freeblocks */

freelength = start->block_length - bytesize;
if (freelength < 4*sizeof(freeblock)) return;

/* Set revised length into what remains, create a length for the
bit to be freed, and free it via the normal function. */

start->block_length = bytesize;
end = (block *)(((uschar *)start) + bytesize);
end->block_length = freelength;
store_free(end + 1);
}

/* End of estore.c */
