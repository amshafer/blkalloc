/**
 * large block allocator - 2017 Austin Shafer
 *
 * A memory pool that is (for the most part) a fixed block allocator
 *
 * this allocates large blocks at one time
 * regular memory can be allocated with blkalloc( size_b size )
 * 
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

#include "blkalloc.h"

typedef struct {
  size_b magic;             // magic number
  size_b size;              // the size of the sub-block
} blkhead;
  
// struct for an smaller sub-block
// for freed pointers in blklarge
struct blksmall_tag {
  blkhead head;             // the sub-block info
  struct blksmall_tag *next;// next sub-block in LL
};

typedef struct blksmall_tag blksmall;

// struct for a single block
struct blklarge_tag {
  int num_free;             // number of freed sub-blocks
  size_b size;              // the size of the block
  size_b next;              // the next location
  size_b fmin;              // smallest freed sub-block
  size_b fmax;              // largest freed sub-block
  size_b fsize;             // the amount of freed space
  void *base;               // the base of the block
  blksmall *freed;          // freed sub-block root (LL)
};

// the struct holding all large blocks
struct blklist_tag {
  unsigned int numb;        // number of blocks
  unsigned int cap;         // block capacity
  blklarge **blockp;        // array of pointers to blocks
};

// global block list
blklist *blist;

/*
 * creates the block header
 * @param the header to set up
 * @param the size of the block
 */
void
blkhead_init (blkhead *bh, size_b s)
{
  bh->magic = MAGIC;
  bh->size = s;
}

/*
 * creates a sub-block free representation
 * @param s the size of the block
 * @param f the position to be freed
 */
void
blksmall_init (size_b s, void *f)
{
  blksmall *bs = (blksmall *)f;
  blkhead_init(&bs->head, (size_b)s);
  bs->next = NULL;
}

/*
 * creates a large block to be allocated into
 * @param s the size of the block
 * @return pointer to block struct (blklarge)
 */
blklarge *
blklarge_init (size_b s)
{
  blklarge *b = BLK_ALLOC(sizeof(blklarge));
  b->size = s;
  b->next = 0;
  b->base = mmap(0, s, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, -1, 0);

  b->num_free = 0;
  b->fmin = 0;
  b->fmax = 0;
  b->fsize = 0;
  b->freed = NULL;
  
  return b;
}

/*
 * frees the blklarge struct
 */
void
blklarge_free (blklarge *bl)
{
  munmap(bl->base, bl->size);
}
  
/*
 * creates the list of blocks in use
 * only initializes the first block
 *
 */
blklist *
blklist_init ()
{
  blklist *bl = BLK_ALLOC(sizeof(blklist));
  bl->numb = 1;
  bl->cap = BLOCK_NUM;
  bl->blockp = BLK_ALLOC(sizeof(blklarge *) * BLOCK_NUM);

  //allocate first block only
  *bl->blockp = blklarge_init(BLOCK_SIZE);

  return bl;
}

/*
 * adds a block to the blist
 * resizes the blockp array if needed
 */
void
blklist_addblock ()
{
  if(blist->numb + 1 > blist->cap) {
    blist->cap *= 2;
    blist->blockp = realloc(blist->blockp, sizeof(blklist) * blist->cap);
  }
  blist->blockp[blist->numb] = blklarge_init(BLOCK_SIZE);
  blist->numb++;
}

/*
 * frees a blklist struct
 */
void
blklist_free (blklist *bls)
{
  for(int i = 0; i < bls->numb; i++) {
    blklarge_free(bls->blockp[i]);
  }
  BLK_FREE(bls->blockp);
}

/*
 * checks the freed pointers in all blocks to see 
 * if there is a open spot that will fit this size
 * if there is, return the address and remove it
 * from freed list
 * search the blocks LL for any open positions
 * @param size the size of the requested allocation
 * @return a pointer to sub-block with available space, or null if not found
 */
blksmall *
find_free (blklarge *bl, size_b size)
{
  // make sure blist is valid
  // find best fit
  blksmall *cur= bl->freed;
  if(cur->head.size >= size) {
    // remove the bucket
    bl->freed = cur->next;
    bl->fsize -= cur->head.size + sizeof(blkhead);
    if(!cur->next) {
      bl->fmin = 0;
      bl->fmax = 0;
    } else {
      bl->fmin = cur->next->head.size;
    }
    bl->num_free--;
    return cur;
  }
  while(cur->next->head.size < size) {
    if(cur->next->next) {
      cur = cur->next;
    } else {
      return NULL;
    }
  }
  // remove the bucket
  blksmall *ret = cur->next;
  if(cur->next->next) {
    cur->next = cur->next->next;
  } else {
    cur->next = NULL;
    bl->fmax = cur->head.size;
  }
  bl->num_free--;
  bl->fsize -= ret->head.size;
  return ret;
}

/*
 * the block allocation call
 * a memory pool that is (for the most part) a
 * fixed size block allocator
 * 
 * @param size the size of the allocated
 * @return a pointer to the available memory in a block
 */
void *
blkalloc (size_b size)
{
  // initilize if needed
  if(!blist) {
    blist = blklist_init();
  }

  // find the current block
  blklarge *cur_block = blist->blockp[0];
  for(int i = 0; i <= blist->numb; i++) {
    if(size >= cur_block->fmin && size <= cur_block->fmax) {
      blkhead *h = (blkhead *)find_free(cur_block, size);
      // h + size of blkhead 
      return h + 1;
    }

    if(cur_block->next + size + sizeof(blkhead) > cur_block->size) {
      if(i == blist->numb) {
	//create new block if needed
	blklist_addblock();
      }
      cur_block = blist->blockp[i];
    }
  }

  // calculate next available position
  // increment next position
  void *addr = cur_block->base;
  addr += cur_block->next;
  blkhead_init(addr, size);
  cur_block->next += size + sizeof(blkhead);
  
  return addr + sizeof(blkhead);
}

/*
 * finds the block that the pointer belongs to
 * @param ptr the pointer in question
 * @return a pointer to the owning block
 */
blklarge *
find_owning_block (void *ptr)
{
  // figure out what block ptr is in
  blklarge *bl = *blist->blockp;
  for(int i = 0; i < blist->numb; i++) {
    if(ptr >= bl->base && ptr < (bl->base + bl->size)) {
      return bl;
    }
    bl++;
  }
  return NULL;
}

/*
 * adjusts blklarge next field to stop block fragmentation
 * if freed spot is at the end then move next back
 * @param bl the block
 * @param h the header struct
 */
int
last_allocated (blklarge *bl, blkhead *h)
{
  size_b h_size = h->size + sizeof(blkhead);
  if((void *)h + h_size == bl->base + bl->next) {
    bl->next -= h_size;
    return 1;
  }
  return 0;
}

/*
 * the free allocated call
 * used to free INDIVIDUAL allocations 
 * NOT entire blocks
 * @param the ptr to be freed
 * @return if found
 */
int
blkfree (void *ptr)
{
  // look up ptr size
  blkhead *h = ptr - sizeof(blkhead);
  if(h->magic != MAGIC) {
    fprintf(stderr, "memory boundaries corrupted\n");
    return -1;
  }
  blklarge *bl = find_owning_block(ptr);
  if(!bl) {
    return -1;
  }

  // if the next field of blklarge is immediatly after
  // then just decrement next by size
  if(last_allocated(bl, h)) {
    return 0;
  }
  
  // add ptr address to freed
  if(h->size >= sizeof(blksmall)) {
    blksmall *fptr = (blksmall *)h;
    blksmall_init(h->size, fptr);
    if(!bl->freed || h->size < bl->freed->head.size) {
      bl->fmin = h->size;
      if(!bl->freed) {
	bl->fmin = h->size / 2;
	bl->fmax = h->size;
      }
      fptr->next = bl->freed;
      bl->freed = fptr;
    } else {
      blksmall *cur = bl->freed;
      while(cur->next != NULL && h->size > cur->next->head.size) {
	cur = cur->next;
      }
      fptr->next = cur->next;
      if(!fptr->next) {
	bl->fmax = h->size;
      }
      cur->next = fptr;
    }
    
    bl->fsize += h->size;
    bl->num_free++;
  }
  return 0;
}

/*
 * frees all blocks and blist variable
 * everything will be freed
 */
void
blkfree_all ()
{
  if(blist)
    blklist_free(blist);
}
