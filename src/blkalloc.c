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

/*
 * data header for allocations. holds the size
 * of the individual allocation
 * magic will be MAGIC if allocated or FREE_MAGIC
 * if freed
 */
typedef struct {
  size_b bh_magic;             // magic number
  size_b bh_size;              // the size of the sub-block
} blkhead;
  
// struct for an smaller sub-block
// for freed pointers in blklarge
struct blksmall_tag {
  blkhead bs_head;             // the sub-block info
  struct blksmall_tag *bs_next;// next sub-block in LL
};

typedef struct blksmall_tag blksmall;

/*
 * struct for a single block
 * this holds a linked list of freed sub blocks which
 * are used if the allocated amount n 
 * satisfies : fmin < n < fmax
 */
struct blklarge_tag {
  int bl_num_free;             // number of freed sub-blocks
  size_b bl_size;              // the size of the block
  size_b bl_next;              // the next location
  size_b bl_fmin;              // smallest freed sub-block
  size_b bl_fmax;              // largest freed sub-block
  size_b bl_fsize;             // the amount of freed space
  void *bl_base;               // the base of the block
  blksmall *bl_freed;          // freed sub-block root (LL)
};

/*
 * the struct holding all large blocks
 * these large blocks (blklarge) will be partitioned 
 * into sub-blocks
 */
struct blklist_tag {
  unsigned int b_numb;        // number of blocks
  unsigned int b_cap;         // block capacity
  blklarge **b_blockp;        // array of pointers to blocks
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
  bh->bh_magic = MAGIC;
  bh->bh_size = s;
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
  blkhead_init(&bs->bs_head, (size_b)s);
  bs->bs_next = NULL;
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
  b->bl_size = s;
  b->bl_next = 0;
  b->bl_base = mmap(0, s, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, -1, 0);

  b->bl_num_free = 0;
  b->bl_fmin = 0;
  b->bl_fmax = 0;
  b->bl_fsize = 0;
  b->bl_freed = NULL;
  
  return b;
}

/*
 * frees the blklarge struct
 */
void
blklarge_free (blklarge *bl)
{
  munmap(bl->bl_base, bl->bl_size);
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
  bl->b_numb = 1;
  bl->b_cap = BLOCK_NUM;
  bl->b_blockp = BLK_ALLOC(sizeof(blklarge *) * BLOCK_NUM);

  //allocate first block only
  *bl->b_blockp = blklarge_init(BLOCK_SIZE);

  return bl;
}

/*
 * adds a block to the blist
 * resizes the blockp array if needed
 */
void
blklist_addblock ()
{
  if(blist->b_numb + 1 > blist->b_cap) {
    blist->b_cap *= 2;
    blist->b_blockp = realloc(blist->b_blockp, sizeof(blklist) * blist->b_cap);
  }
  blist->b_blockp[blist->b_numb] = blklarge_init(BLOCK_SIZE);
  blist->b_numb++;
}

/*
 * frees a blklist struct
 */
void
blklist_free (blklist *bls)
{
  for(int i = 0; i < bls->b_numb; i++) {
    blklarge_free(bls->b_blockp[i]);
  }
  BLK_FREE(bls->b_blockp);
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
  blksmall *cur= bl->bl_freed;
  if(cur->bs_head.bh_size >= size) {
    // remove the bucket
    bl->bl_freed = cur->bs_next;
    bl->bl_fsize -= cur->bs_head.bh_size + sizeof(blkhead);
    if(!cur->bs_next) {
      bl->bl_fmin = 0;
      bl->bl_fmax = 0;
    } else {
      bl->bl_fmin = cur->bs_next->bs_head.bh_size;
    }
    bl->bl_num_free--;
    return cur;
  }
  while(cur->bs_next->bs_head.bh_size < size) {
    if(cur->bs_next->bs_next) {
      cur = cur->bs_next;
    } else {
      return NULL;
    }
  }
  // remove the bucket
  blksmall *ret = cur->bs_next;
  if(cur->bs_next->bs_next) {
    cur->bs_next = cur->bs_next->bs_next;
  } else {
    cur->bs_next = NULL;
    bl->bl_fmax = cur->bs_head.bh_size;
  }
  bl->bl_num_free--;
  bl->bl_fsize -= ret->bs_head.bh_size;
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
  blklarge *cur_block = blist->b_blockp[0];
  for(int i = 0; i <= blist->b_numb; i++) {
    if(size >= cur_block->bl_fmin && size <= cur_block->bl_fmax) {
      blkhead *h = (blkhead *)find_free(cur_block, size);
      // h + size of blkhead 
      return h + 1;
    }

    if(cur_block->bl_next + size + sizeof(blkhead) > cur_block->bl_size) {
      if(i == blist->b_numb) {
	//create new block if needed
	blklist_addblock();
      }
      cur_block = blist->b_blockp[i];
    }
  }

  // calculate next available position
  // increment next position
  void *addr = cur_block->bl_base;
  addr += cur_block->bl_next;
  blkhead_init(addr, size);
  cur_block->bl_next += size + sizeof(blkhead);
  
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
  blklarge *bl = *blist->b_blockp;
  for(int i = 0; i < blist->b_numb; i++) {
    if(ptr >= bl->bl_base && ptr < (bl->bl_base + bl->bl_size)) {
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
  size_b h_size = h->bh_size + sizeof(blkhead);
  if((void *)h + h_size == bl->bl_base + bl->bl_next) {
    bl->bl_next -= h_size;
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
  if(h->bh_magic != MAGIC) {
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
  if(h->bh_size >= sizeof(blksmall)) {
    blksmall *fptr = (blksmall *)h;
    blksmall_init(h->bh_size, fptr);
    fptr->bs_head.bh_magic = FREE_MAGIC;
    if(!bl->bl_freed || h->bh_size < bl->bl_freed->bs_head.bh_size) {
      bl->bl_fmin = h->bh_size;
      if(!bl->bl_freed) {
	bl->bl_fmin = h->bh_size / 2;
	bl->bl_fmax = h->bh_size;
      }
      fptr->bs_next = bl->bl_freed;
      bl->bl_freed = fptr;
    } else {
      blksmall *cur = bl->bl_freed;
      while(cur->bs_next != NULL && h->bh_size > cur->bs_next->bs_head.bh_size) {
	cur = cur->bs_next;
      }
      fptr->bs_next = cur->bs_next;
      if(!fptr->bs_next) {
	bl->bl_fmax = h->bh_size;
      }
      cur->bs_next = fptr;
    }
    
    bl->bl_fsize += h->bh_size;
    bl->bl_num_free++;
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
