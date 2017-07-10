/*
 * tests the blkalloc allocator
 * Austin Shafer
 *
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/blkalloc.h"

#define ALLOCATOR( s ) blkalloc( (s) )
#define FREE( p ) blkfree( (p) )

void
basic_test ()
{
  char *name = ALLOCATOR(strlen("Austin Shafer") + 1);
  strcpy(name, "Austin Shafer");
  
  printf("My allocated name is %s\n", name);

  FREE(name);
  printf("allocated name is now freed\n");

  char *second_name = ALLOCATOR(strlen("Kelby Beam") + 1);
  strcpy(second_name, "Kelby Beam");

  printf("My second allocated name is %s\n", second_name);

  FREE(second_name);
  printf("second allocated name is now freed\n");
}

void
test_count (int c)
{
  char *nums[c];
  for(int i = 0; i < c; i++) {
    nums[i] = ALLOCATOR(sizeof(char) + 15);
  }

  for(int i = 0; i < c; i++) {
    FREE(nums[i]);
  }
  printf("allocated and freed %d sub-blocks\n", c);
}

int
main (int argc, char *argv[])
{
  basic_test();
  test_count(1000000);
  test_count(10);
  
  blkfree_all();
  return 0;
}
