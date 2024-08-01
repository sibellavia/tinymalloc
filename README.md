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

pull requests are welcome if you want to improve the code
