/**
 * large block allocator - 2017 Austin Shafer
 *
 * A memory pool that is (for the most part) a fixed block allocator
 *
 * this allocates large blocks at one time
 * regular memory can be allocated with blkalloc( size_t size )
 * 
 */

#include <stdlib.h>
#include <stdbool.h>

#include "blkalloc.h"

// struct for an smaller sub-block
// for freed pointers in blklarge
typedef struct {
  size_t size;              // size of sub-block
  void *base;               // base of sub-block
  blksmall *next;           // next sub-block in LL
} blksmall;

// struct for a single block
struct blklarge_tag
{
  int num_free;             // number of freed sub-blocks
  size_t size;              // the size of the block
  size_t next;              // the next location
  size_t fmin;              // smallest freed sub-block
  size_t fmax;              // largest freed sub-block
  size_t fsize;             // the amount of freed space
  void *base;               // the base of the block
  blksmall *freed;          // freed sub-block root (LL)
};

// the struct holding all large blocks
struct blklist_tag
{
  unsigned int numb;        // number of blocks
  blklarge **blockp;        // array of pointers to blocks
};

// global block list
extern blklist *blist;

/*
 * creates a sub-block to be allocated into
 * @param s the size of the block
 * @param f the position to be freed
 * @return pointer to block struct (blklarge)
 */
blksmall *
blksmall_init (size_t s, void *f)
{
  sb = ALLOC(sizeof(blksmall));
  sb->size = s;
  sb->base = f;
  sb->next = NULL;
}

/*
 * creates a large block to be allocated into
 * @param s the size of the block
 * @return pointer to block struct (blklarge)
 */
blklarge *
blklarge_init (size_t s)
{
  b = ALLOC(sizeof(blklarge));
  b->size = s;
  b->next = 0;
  b->base = ALLOC(sizeof(char) * s);

  b->num_free = 0;
  b->fmin = -1;
  b->fmax = -1;
  b->fsize = 0;
  b->freed = NULL;
  
  return b;
}

/*
 * creates the list of blocks in use
 * only initializes the first block
 *
 */
blklist *
blklist_init ()
{
  bl = ALLOC(sizeof(blklist));
  bl->numb = 1;
  bl->blockp = ALLOC(sizeof(*blklarge) * BLOCK_NUM);

  //allocate first block only
  bl->blockp->base = blklarge_init(BLOCK_NUM);

  return bl;
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
find_free (size_t size)
{
  for(int i = 0; i < blist->numb; i++) {
    if(size >= blist->blockp[i]->fmin && size <= blist->blockp[i]->fmax) {
      // find best fit
      blksmall *cur= blist->blockp[i]->freed;
      if(cur->size >= size) {
	// remove the bucket
	blist->blockp[i]->freed = cur->next;
	blist->blockp[i]->fsize -= cur->size;
	blist->blockp[i]->fmin = cur->next->size;
	return cur;
      }
      while(cur->next->size < size) {
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
	blist->blockp[i]->fmax = cur->size;
      }
      blist->blockp[i]->fsize -= ret->size;
      return ret;
    }
  }
  return NULL;
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
blkalloc (size_t size)
{
  blksmall *found = find_free(size);
  if(found) {
    return found;
  }

  // find the current block
  // calculate next available position
  // increment next position
  cur_block = blist->blockp[blist->numb];
  void *addr = cur_block->base;
  addr += cur_block->next;
  cur_block->next += size;
  
  return addr;
}
