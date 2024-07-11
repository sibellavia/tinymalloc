/* Heap Initialization
- Create an initial heap of a fixed size (e.g, 1 MB)
- Set up the initial free block
*/

#include <stdio.h>
#include <stddef.h>

#define INITIAL_HEAP_SIZE (1024 * 1024)
#define NULL 0

struct block {
    size_t size; // Size of the block
    int is_free; // 1 if free, 0 if allocated
    struct block *next; // Pointer to the next block
} *heap_start;

void initialize_heap(){
    void *initial_mem = sbrk(INITIAL_HEAP_SIZE);
    if (initial_mem == (void*)-1){
        // Handle error
        return;
    }

    heap_start = (struct block*)initial_mem;
    heap_start->size = INITIAL_HEAP_SIZE - sizeof(struct block);
    heap_start->is_free = 1;
    heap_start->next = NULL;
};

void *tinymalloc(size_t size){
    struct block *current = heap_start;

    while(current){
        if(current->is_free && current->size >= size){
            // We found a suitable block
            current->is_free = 0;
        }
    }
}