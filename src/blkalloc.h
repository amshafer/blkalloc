/**
 * large block allocator - 2017 Austin Shafer
 *
 * this allocates large blocks at one time
 * regular memory can be allocated with blkalloc( size_t size )
 * 
 */

#ifndef BLKALLOC_H
#define BLKALLOC_H

#include <stdlib.h>

//--------------------------------------------- constants 

// default size of the blocks
#define BLOCK_SIZE 131072
// starting number of blocks
#define BLOCK_NUM 1
// min block size
#define BLOCK_MIN 16

// magic for header identification
#define MAGIC 45916
#define FREE_MAGIC 45917

//--------------------------------------------- macro defintions

// OS specific alloc call
#define BLK_ALLOC( size ) malloc( (size) )

// OS specific free call
#define BLK_FREE( p ) free( (p) ) 

//--------------------------------------------- function prototypes

// structs used for holding 
typedef struct blklist_tag blklist_t;


// max size of allocation
typedef unsigned int size_b;

/*
 * the block allocation call
 * a memory pool that is (for the most part) a
 * fixed size block allocator
 * 
 * @param size the size of the allocated
 * @return a pointer to the available memory in a block
 */
void * blkalloc (size_b size);

/*
 * the free allocated call
 * used to free INDIVIDUAL allocations 
 * NOT entire blocks
 * @param the ptr to be freed
 * @return if found
 */
int blkfree (void *ptr);

/*
 * frees all blocks and blist variable
 * everything will be freed
 */
void blkfree_all ();

//---------------------------------------------
#endif
