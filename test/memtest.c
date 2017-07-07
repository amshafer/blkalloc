/*
 * tests the blkalloc allocator
 * Austin Shafer
 *
 *
 */

#include <string.h>
#include <stdio.h>
#include "../src/blkalloc.h"

void
basic_test ()
{
  char *name = blkalloc(strlen("Austin Shafer") + 1);
  strcpy(name, "Austin Shafer");
  
  printf("My allocated name is %s\n", name);

  blkfree(name);
  printf("allocated name is now freed\n");

  char *second_name = blkalloc(strlen("Kelby Beam") + 1);
  strcpy(second_name, "Kelby Beam");

  printf("My second allocated name is %s\n", second_name);

  blkfree(second_name);
  printf("second allocated name is now freed\n");
}

void
test_count (int c)
{
  int *nums[c];
  for(int i = 0; i < c; i++) {
    nums[i] = blkalloc(sizeof(int));
  }

  for(int i = 0; i < c; i++) {
    blkfree(nums[i]);
  }
  printf("allocated and freed %d sub-blocks\n", c);
}

int
main (int argc, char *argv[])
{
  //basic_test();
  test_count(1000);
  
  blkfree_all();
  return 0;
}
