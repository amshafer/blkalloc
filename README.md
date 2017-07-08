# blkalloc
A primitive block allocator and associated programs

  This allocator uses a perfect fit model as opposed to a power of two allocator. 
Large blocks are created and then cut into sub-blocks as "blkalloc" calls are
made, allowing allocations of almost any size. 
  At the current moment, the worst case memory overhead is less than around 20%.
This is liable to change as development continues.
