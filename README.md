# blkalloc
A primitive block allocator and associated programs - Work In Progress

run "make test" in base directory to view results of existing tests

  This allocator uses a first fit model as opposed to a power of two allocator. 
Large blocks are created and then cut into sub-blocks as "blkalloc" calls are
made, allowing allocations of almost any size. 
  At the current moment, this project is being redesigned due to poor original design
and regression in passing tests. This is liable to change as development continues.

TODO -
     - allocater is turbulent when more than one blocks worth of memory is allocated
     - regression in random sequence testing (memtest.c:test_rand) debug
     - valgrind and general airtight testing needed once existed tests are working