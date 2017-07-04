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
#define BLOCK_SIZE 512
// starting number of blocks
#define BLOCK_NUM 5

//--------------------------------------------- macro defintions

// OS specific alloc call
#define BLK_ALLOC( size ) malloc( (size) )

// OS specific free call
#define BLK_FREE( p ) free( (p) ) 

//--------------------------------------------- function prototypes

// structs used for holding 
typedef struct blklist_tag blklist;
typedef struct blklarge_tag blklarge;

void * blkalloc (size_t size);

//---------------------------------------------
#endif
