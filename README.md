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

- [x] v0.1 (basic implementation)

  - basic allocation and deallocation
  - simple first-fit algorithm
  - rudimentary block splitting
  - basic error checking
  - basic heap initialization
  - simple coalescing
  - basic thread safety with global mutex
  - replace sbrk() with mmap()

- [x] v0.2 (bitmap allocation implementation)

  - define constants and data structures for bitmap allocator
  - implement basic bitmap manipulation functions
  - implement memory allocation function (tinymalloc) with first-fit algorithm
  - implement memory deallocation function (tinyfree)
  - implement initialization function
  - implement heap extension functionality
  - switch to 64-bit bitmap for improved efficiency
  - optimize allocation algorithm using __builtin_ffsll
  - add basic debugging output

- [] v0.3 (enhanced bitmap allocation)

  - implement best-fit allocation algorithm
  - add support for storing allocation size with each block
  - implement strategies to reduce fragmentation
  - optimize bitmap operations

- [] v0.4 (advanced features)

  - implement realloc()
  - add support for different block sizes or multiple heaps
  - implement strategies to reduce external fragmentation
  - enhanced thread safety with finer-grained locking

- [] v0.5 (multi-arena implementation)

  - design and implement a multi-arena structure
  - create a simple arena selection mechanism (thread ID?)
  - modify allocation and deallocation functions to work with multiple arenas
  - implement basic load balancing between arenas

- [] v0.6 (per-cpu arena optimization)

  - extend multi-arena implementation to support per-cpu arenas
  - implement arena indexing using sched_getcpu()
  - handle cross-cpu frees and memory migration between arenas

- [] v0.7 (extended functionality)

  - implement debugging and memory leak detection
  - implement calloc()
  - implement memory pools or arenas

- [] v0.8 (system integration)

- [] v0.9 (pre-release polish)

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
