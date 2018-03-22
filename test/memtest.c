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
#define CALLOCATOR( s ) blkcalloc( 1, (s) )
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
  char **nums;
  nums = ALLOCATOR(sizeof(char *) * c);
  for(int i = 0; i < c; i++) {
    nums[i] = ALLOCATOR(sizeof(char) + 15);
  }

  for(int i = 0; i < c; i++) {
    FREE(nums[i]);
  }
  printf("allocated and freed %d sub-blocks\n", c);
  FREE(nums);
}

void
gen_rand (char *name, int n, int s)
{
  FILE *fp = fopen(name, "w");
  fprintf(fp, "%d %d\n", n, s);
  char *on[s];
  for(int i = 0; i < n; i++) {
    int slot = rand() % s;
    if(on[slot]) {
      fprintf(fp, "f %d\n", slot);
      on[slot] = NULL;
    } else {
      int size = rand() % (BLOCK_SIZE / 2 - 4) + 4;
      fprintf(fp, "a %d %d\n", slot, size);
    }
  }

  for(int top = 0; top < s; top++) {
    if(on[top]) {
       fprintf(fp, "f %d\n", top);
    }
  }
}

void
test_rand (char *name)
{
  FILE *fp = fopen(name, "r");
  int n, s = 0;
  fscanf(fp, "%d %d", &n, &s);
  char **on = CALLOCATOR(sizeof(char *) * s);

  for(int i = 0; i < n; i++) {
    char c = fgetc(fp);
    if(c == 'a') {
      int slot, size = 0;
      fscanf(fp, "%d %d", &slot, &size);
      if (on[slot] == NULL) {
	printf("Allocating bucket of size %d\n", size);
	on[slot] = ALLOCATOR(size);
      }
    } else if(c == 'f') {
      int slot = 0;
      fscanf(fp, "%d", &slot);
      printf("Freeing slot %d\n", slot);
      FREE(on[slot]);
      on[slot] = NULL;
    }
  }

  for (int i = 0; i < s; i++) {
    FREE(on[i]);
  }
  FREE(on);
}

int
main (int argc, char *argv[])
{
  //basic_test();
  test_count(50000);
  test_count(10);

  char name[] = "test/r01.in";
  //gen_rand(name, 100, 50);
  test_rand(name);
  
  blkfree_all();
  return 0;
}
