#include <stdio.h>
#include <stddef.h>
#include <unistd.h>

#define INITIAL_HEAP_SIZE (1024 * 1024)
#define EXPANSION_SIZE (1024 * 1024)

struct block {
    size_t size; // Size of the block
    int is_free; // 1 if free, 0 if allocated
    struct block *next; // Pointer to the next block
} *heap_start;

void *tinymalloc(size_t size){
    struct block *current = heap_start;

    while(current){
        if(current->is_free && current->size >= size){
            // We found a suitable block
            current->is_free = 0;
            if(current->size > size + sizeof(struct block) + 16){
                struct block *new_block = (struct block*)((char*)current + sizeof(struct block) + size);
                new_block->size = current->size - size - sizeof(struct block);
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            return (void*)(current + 1); // return pointer to usable memory
        }
        current = current->next;
    }

    // No suitable block found, request more memory
    void *more_mem = sbrk(EXPANSION_SIZE);
    if(more_mem == (void*)-1){
        // Unable to allocate memory
        return NULL;
    }

    // Set up new block
    struct block *new_block = (struct block*)more_mem;
    new_block->size = EXPANSION_SIZE - sizeof(struct block);
    new_block->is_free = 1;
    new_block->next = NULL;

    // Add new block to the end of the list
    struct block *last = heap_start;
    while (last->next != NULL){
        last = last->next;
    }
    last->next = new_block;

    tinymalloc(size);
}
