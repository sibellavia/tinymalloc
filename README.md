# tinymalloc

tinymalloc is a basic implementation of a memory allocator, inspired by the standard malloc library.

it provides fundamental memory allocation and deallocation functionality.

i did entirely for fun and for didactic purposes. i started writing in C several years ago at a basic level. now i want to improve my skills and reach higher levels. that is why i started this small project.

## features

- minimal memory allocation (tinymalloc)
- minimal memory deallocation (tinyfree)
- block splitting
- basic coalescing of free blocks
- basic alignment of allocated memory

## areas of improvement

as you will see from the code, tinymalloc isn't to be intended for prod or as a totally complete implementation. it has many areas for improvement!

in fact, i didn't implement realloc and calloc, thread safety (current implementation is not thread-safe), tunable parameters ... and other advanced features

i can't guarantee it, but in the future it would be fun to implement a better error handling, a more complex memory alignment, heap fragmentation handling, and other performance optimizations :-)

## roadmap

- [] v0.1 (basic implementation)

  - basic allocation and deallocation
  - simple first-fit algorithm
  - rudimentary block splitting
  - basic error checking
  - basic heap initialization
  - simple coalescing (only with next block)
  - basic thread safety with global mutex
  - replace sbrk() with mmap()

- [] v0.2 (improved memory management)

  - refine block splitting algorithm
  - implement proper coalescing (both with previous and next blocks)
  - improve free list management
  - enhance error handling

- [] v0.3 (memory alignment and optimization)

  - implement proper memory alignment
  - optimize allocation and deallocation with contingency scenarios
  - basic performance benchmarking

- [] v0.4 (advanced allocation strategies)

  - implement more efficient free block management
  - refine the allocation algorithm

- [] v0.5 (thread safety improvements)

  - implement finer-grained locking
  - extensive testing in multi-threaded environments

- [] v0.6 (advanced features)

  - implement realloc()
  - implement calloc()
  - implement simple strategies to reduce fragmentation

- [] v0.7 (robustness and testing)

  - comprehensive test suite
  - stress testing and edge case handling
  - basic memory leaking detection capabilities?

- [] v0.8 (performance optimization)

  - free list binning
  - extensive performance comparisons with other memory allocators

- [] v0.9 (pre-release)

  - code cleanup
  - documentation
  - final optimizations based on benchmarks

- [] v1.0 (stable)

## contributing

pull requests are welcome if you want to improve the code :-)

here are some suggestions:

- use 2 spaces for indentation
- use meaningful variable and function names
- comment all functions with a brief description
- add inline comments where possible
- write unit tests for new features
- always check for buffer overflows and memory leaks
- maintain compatibility with C11
