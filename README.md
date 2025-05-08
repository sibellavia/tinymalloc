# tinymalloc: a compact memory allocator

tinymalloc is a neat little memory allocator I put together, drawing inspiration from the good old `malloc` we all know from the standard C library.

This project is all about providing the essentials for memory allocation and deallocation. I embarked on this journey primarily for the fun of it. 

## What tinymalloc can do

-   **Bitmap-Powered Allocation**: at its core, tinymalloc uses a bitmap to keep track of memory blocks. When you ask for memory, it employs a first-fit algorithm to find a suitable spot.
-   **Straightforward Deallocation**: `tinyfree` releases memory back to the system.
-   **Multi-Arena Architecture**: to enhance performance in multi-threaded environments, tinymalloc utilizes multiple memory arenas. Each CPU core gets its own arena.
-   **Thread-Safety (Global Lock)**: for now, thread safety is ensured by a global mutex (`malloc_mutex`).
-   **Dynamic Heap Expansion**: if an arena runs out of space, tinymalloc can request more memory from the system, extending the heap on the fly.
-   **Small, Medium, and Large Allocations**: the allocator tries to be smart about how it finds free blocks, using different strategies based on the size of the requested allocation.

## Room for growth 

As you might guess from the "tiny" in tinymalloc, this isn't a production-ready allocator. Yet there's plenty of room for improvement.

For instance, I haven't implemented `realloc` or `calloc`. Also, the current thread-safety model, while functional, could be more granular. There are no tunable parameters at runtime, and many advanced features commonly found in mature allocators are still on the to-do list.

Looking ahead, it would be great to dive into:

-   More sophisticated error handling.
-   Finer-grained memory alignment controls.
-   Strategies to combat heap fragmentation.
-   Various performance optimizations to make it even zippier!

## Collaborating

If you're thinking of contributing, here are a few friendly guidelines:

-   Clear, concise inline comments are always appreciated, especially for the tricky bits.
-   If you add new features, please consider writing unit tests.
-   And, of course, always be mindful of potential buffer overflows and memory leaks.
