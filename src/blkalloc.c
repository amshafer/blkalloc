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

// NOTATION: a memory 'bucket' is a hole or zombie 
// holes are available memory buckets

/*
 * data header for allocations. holds the size
 * of the individual allocation
 * magic will be MAGIC if allocated or FREE_MAGIC
 * if freed
 */
struct blkhead_tag {
  size_b bh_magic;             // magic number
  size_b bh_size;              // the size of the sub-block
  struct blkhead_tag *bh_next; // the next memory bucket
};

typedef struct blkhead_tag blkhead_t;

/*
 * the struct holding all large blocks
 * these large blocks (blklarge) will be partitioned 
 * into sub-blocks
 */
struct blklist_tag {
  unsigned int b_numh;        // number of holes
  unsigned int b_numb;        // number of blocks
  blkhead_t *b_blocks;        // mmaped blocks that need freeing
  blkhead_t *b_holes;         // free blocks linked list
};

// global block list
blklist_t *blist;

/*
 * creates the block header
 * @param the header to set up
 * @param the size of the block
 */
void
blkhead_init (blkhead_t *bh, size_b s, blkhead_t *next)
{
  bh->bh_magic = MAGIC;
  bh->bh_size = s;
  bh->bh_next = next;
}

/*
 * cuts blksmall into the right size
 * if the hole is larger than the size, then
 * break it into two small blocks, placing the new
 * one at the front of the list
 * 
 * @param bs the block to break
 * @param size the size of the requested allocation
 */
void
blkhead_break (blkhead_t *bh, size_b size)
{
  // don't split if block would be too small
  if (bh->bh_size - size <= BLOCK_MIN) {
    return;
  }

  // create new block header
  void *new_block = (void *)bh + sizeof(blkhead_t) + size;
  blkhead_t *h = (blkhead_t *)new_block;

  // add new split block to holes
  blkhead_init(h, bh->bh_size - sizeof(blkhead_t) - size, blist->b_holes);
  blist->b_holes = h;
  blist->b_numh++;

  // fix old hole size after breaking
  bh->bh_size = size;
}


/*
 * Used to clean up the blksmall struct from the list when
 * a block is returned to the user.
 ***** DOES NOT handle bs_next entries. YOU must do that yourself
 * @param bs the small struct to operate on
 * @return a pointer to the allocated data
 */
void *
blkhead_get_base (blkhead_t *bh, size_b size)
{
  void *ret = bh;
  blkhead_break(bh, size);

  return ret + sizeof(blkhead_t);
}

/*
 * adds a new BLOCK_SIZE hole to the blklist
 * @param bl the blklist to add to
 */ 
void
blklist_addblock (blklist_t *bl)
{
  void *new_block = BLK_ALLOC(BLOCK_SIZE + sizeof(blkhead_t) * 2);
  // add malloc'd to b_blocks
  blkhead_init(new_block, BLOCK_SIZE + sizeof(blkhead_t), bl->b_blocks);
  bl->b_blocks = (blkhead_t *)new_block;
  // add BLOCK_SIZE section to b_holes
  blkhead_init(new_block + sizeof(blkhead_t), BLOCK_SIZE, bl->b_holes);
  bl->b_holes = (blkhead_t *)(new_block + sizeof(blkhead_t));
}

/*
 * creates the list of blocks in use
 * only initializes the first block
 *
 */
blklist_t *
blklist_init ()
{
  blklist_t *bl = BLK_ALLOC(sizeof(blklist_t));

  // create all starting blocks
  bl->b_numh = BLOCK_NUM;
  bl->b_numb = BLOCK_NUM;  
  bl->b_holes = bl->b_blocks = NULL;

  // b_blocks holds malloc'd blocks
  blklist_addblock(bl);

  return bl;
}

/*
 * frees a blklist struct
 * @param bls the list to free
 * @return void
 */
void
blklist_free (blklist_t *bls)
{
  blkhead_t *b = bls->b_blocks;
  while (b) {
    blkhead_t *next = b->bh_next;
    free(b);
    b = next;
  }
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
  // initilize if neededp
  if (!blist) {
    blist = blklist_init();
  }

  // if larger than block size just mmap it
  if (size + sizeof(blkhead_t) >= BLOCK_SIZE) {
    // make a header for it so we know how to free it
    void *ret = BLK_ALLOC(size + sizeof(blkhead_t));
    blkhead_init(ret, size, NULL);
    return ret + sizeof(blkhead_t);
  }

  // if we are out of available memory buckets
  if (!blist->b_holes) {
    // make new block if needed
    blklist_addblock(blist);    
  }
  
  // search for first block available
  blkhead_t *bh = blist->b_holes;
  while (bh) { 
    if (bh->bh_size > size) {
      // remove from holes
      blist->b_holes = bh->bh_next;
      bh->bh_next = NULL;
      blist->b_numh--;

      return blkhead_get_base(bh, size);
    }
    
    bh = bh->bh_next;
  }

  // new block is first in list so break it if needed
  return blkhead_get_base(blist->b_holes, size);
}

/*
 * the block callocation call
 * same as blkalloc but zero's the 
 * allocated memory
 * 
 * @param size the size of the allocated
 * @return a pointer to the available memory in a block
 */
void * 
blkcalloc (size_b size)
{
  void *ret = blkalloc(size);
  memset(ret, 0, size);
  return ret;
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
  if (!ptr) {
    fprintf(stderr, "blkalloc: attempting to free a null pointer\n");
    fprintf(stderr, "blkalloc: (ignoring null pointer free)\n");
    return -1;
  }
  
  // look up ptr size
  blkhead_t *h = ptr - sizeof(blkhead_t);
  if (h->bh_magic != MAGIC) {
    fprintf(stderr, "blkalloc: Memory boundaries corrupted\n");
    fprintf(stderr, "blkalloc: (or pointer is from a different allocator)\n");
    return -1;
  }

  // if h's size is too large then we must have
  // allocating using the std method
  if (h->bh_size + sizeof(blkhead_t) > BLOCK_SIZE) {
    BLK_FREE(h);
    return 0;
  }

  // place h at front of free list
  h->bh_next = blist->b_holes;
  blist->b_holes = h;
  blist->b_numh++;
  
  // maybe sort this?
  
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
