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
} blkhead_t;
  
// struct for an smaller sub-block
// for freed pointers in blklarge
struct blksmall_tag {
  blkhead_t *bs_head;             // the sub-block info
  struct blksmall_tag *bs_next;  // next sub-block in LL
};

typedef struct blksmall_tag blksmall_t;


/*
 * the struct holding all large blocks
 * these large blocks (blklarge) will be partitioned 
 * into sub-blocks
 */
struct blklist_tag {
  unsigned int b_numh;        // number of holes
  unsigned int b_numb;        // number of blocks
  blksmall_t *b_holes;        // free blocks linked list
  void **b_blocks;            // freeable unsplit blocks
  blksmall_t *b_zombies;      // empty blksmall object storage
};

// global block list
blklist_t *blist;

/*
 * creates the block header
 * @param the header to set up
 * @param the size of the block
 */
void
blkhead_init (blkhead_t *bh, size_b s)
{
  bh->bh_magic = MAGIC;
  bh->bh_size = s;
}

/*
 * creates a sub-block free representation
 * @param s the size of the block
 * @param f the position to be freed
 */
blksmall_t *
blksmall_init (size_b s, blksmall_t *next)
{
  blksmall_t *bs = BLK_ALLOC(sizeof(blksmall_t));

  if (s != 0) {
    bs->bs_head = BLK_ALLOC(sizeof(blkhead_t) + BLOCK_SIZE);
    blkhead_init(bs->bs_head, s);
  } else {
    bs->bs_head = NULL;
  }
  
  bs->bs_next = next;

  return bs;
}

/*
 * frees a blksmall struct
 * @param bs the struct to free
 */
void
blksmall_free (blksmall_t *bs)
{
  //BLK_FREE(bs->bs_head);
  BLK_FREE(bs);
}

/*
 * creates a sub-block free representation
 * @param head pointer to the head data to use
 * @param next the next block in the list
 * @return a blksmall off of the zombie pile
 */
blksmall_t *
blksmall_refurbish (blkhead_t *head, blksmall_t *next)
{
  // get a free small block struct
  blksmall_t *ret = blist->b_zombies;
  blist->b_zombies = ret->bs_next;

  ret->bs_head = head;
  ret->bs_next = next;

  return ret;
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
blksmall_break (blksmall_t *bs, size_b size)
{
  if (bs->bs_head->bh_size - size <= BLOCK_MIN) {
    return;
  }

  // create new block header
  void *new_block = (void *)bs->bs_head + sizeof(blkhead_t) + size;
  blkhead_t *h = (blkhead_t *)new_block;
  blkhead_init(h, bs->bs_head->bh_size - sizeof(blkhead_t) - size);

  // create new holen
  blksmall_t *new = blksmall_init(0, blist->b_holes);
  new->bs_head = h;
  blist->b_holes = new;

  // fix old hole size
  bs->bs_head->bh_size = size;

  blist->b_numh++;
}


/*
 * Used to clean up the blksmall struct from the list when
 * a block is returned to the user.
 ***** DOES NOT handle bs_next entries. YOU must do that yourself
 * @param bs the small struct to operate on
 * @return a pointer to the allocated data
 */
void *
blksmall_get_base (blksmall_t *bs, size_b size)
{
  void *ret = bs->bs_head;
  blksmall_break(bs, size);

  // add to pile of zombies
  bs->bs_next = blist->b_zombies;
  blist->b_zombies = bs;

  return ret + sizeof(blkhead_t);
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
  bl->b_holes = blksmall_init(BLOCK_SIZE, NULL);
  bl->b_zombies = NULL;

  // blocks holds malloc'd blocks
  bl->b_blocks = BLK_ALLOC(sizeof(void *) * BLOCK_NUM);
  bl->b_blocks[0] = (void *)(bl->b_holes->bs_head);
  
  blksmall_t *cur = bl->b_holes;
  for (int i = 1; i < BLOCK_NUM; i++) {
    cur->bs_next = blksmall_init(BLOCK_SIZE, NULL);
    cur = cur->bs_next;
    bl->b_blocks[i] = (void *)cur->bs_next->bs_head;
  }

  return bl;
}

/*
 * adds a block to the blist
 * resizes the blockp array if needed
 */
void
blklist_addblock (blklist_t *bl)
{
  blksmall_t *new = blksmall_init(BLOCK_SIZE, bl->b_holes);
  bl->b_holes = new;
}

/*
 * frees a blklist struct
 * @param bls the list to free
 * @return void
 */
void
blklist_free (blklist_t *bls)
{
  for(int i = 0; i < bls->b_numb; i++) {
    free(blist->b_blocks[i]);
  }

  // free all blksmall_t structs
  blksmall_t *bs = bls->b_holes;
  while (bs) {
    blksmall_t *next = bs->bs_next;
    blksmall_free(bs);
    bs = next;
  }

  // array not a linked list
  BLK_FREE(bls->b_blocks);
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
    blkhead_init(ret, size);
    return ret + sizeof(blkhead_t);
  }

  // if we are out of available memory buckets
  if (!blist->b_holes) {
    // make new block if needed
    blklist_addblock(blist);    
  }
  
  // search for first block available
  blksmall_t *bs = blist->b_holes;
  if (bs->bs_head->bh_size > size) {
    blist->b_holes = bs->bs_next;
    blist->b_numh--;
    return blksmall_get_base(bs, size);
  }

  while (bs->bs_next) { 
    if (bs->bs_next->bs_head->bh_size > size) {
      blksmall_t *ret = bs->bs_next;
      bs->bs_next = ret->bs_next;
      blist->b_numh--;
      return blksmall_get_base(ret, size);
    }
    
    bs = bs->bs_next;
  }

  // new block is first in list so break it if needed
  return blksmall_get_base(blist->b_holes, size);
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
    return -1;
  }
  
  // look up ptr size
  blkhead_t *h = ptr - sizeof(blkhead_t);
  if (h->bh_magic != MAGIC) {
    fprintf(stderr, "Memory boundaries corrupted\n");
    fprintf(stderr, "(or pointer is from a different allocator)\n");
    return -1;
  }

  // if h's size is too large then we must have
  // allocating using the std method
  if (h->bh_size + sizeof(blkhead_t) > BLOCK_SIZE) {
    BLK_FREE(h);
    return 0;
  }

  // place h at front of free list
  blist->b_holes = blksmall_refurbish(h, blist->b_holes);
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
