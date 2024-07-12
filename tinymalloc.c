#include "tinymalloc.h"
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>

#define INITIAL_HEAP_SIZE (1024 * 1024)
#define THRESHOLD (1024)
#define EXPANSION_SIZE (1024 * 1024)

/* Block Structure */

struct block {
    size_t size;
    int is_free;
    struct block *next;
} *heap;

/* tinymalloc() */
void *tinymalloc(size_t size){
    if(size == 0) return NULL;

    // Initialize Heap if it hasn't been initialized yet
    if(heap == NULL){
        void *initial_mem = sbrk(INITIAL_HEAP_SIZE);

        // When sbrk() fails to allocate memory, it returns (void*)-1
        // If so, we return NULL as an error
        if(initial_mem == (void*)-1) return NULL;

        // sbrk() returns a void pointer to the start of the newly allocated memory.
        // We cast this to struct block* because we want to use the beginning
        // of this memory to store our block metadata.
        heap = (struct block*)initial_mem;
        heap->size = INITIAL_HEAP_SIZE - sizeof(struct block);
        heap->is_free = 1;
        heap->next = NULL;
    }

    struct block *current = heap;

    while(1){
        while(current){
            if(current->is_free && current->size >= size){
                current->is_free = 0;

                // Size management, we split if it is too much
                if(current->size > size + sizeof(struct block) + THRESHOLD){
                    // Get extra memory
                    size_t extra_memory = current->size - size - sizeof(struct block);

                    // Initialize new block
                    struct block *new_block = (struct block*)((char*)current + sizeof(struct block) + size);

                    new_block->is_free = 1;
                    new_block->size = extra_memory;
                    new_block->next = current->next;

                    current->next = new_block;
                    current->size = size;
                }

                // The reason for returning current + 1 is to skip over the
                // block metadata and return a pointer to the usable memory.
                // When you add 1 to a struct pointer, it advances the pointer
                // by the size of the struct. So current + 1 points to the
                // memory immediately after the block metadata, which is where
                // the usable memory begins.
                return (void*)(current + 1); // We return pointer to usable memory
            }

            current = current->next;
        }

        // No suitable memory found, we request more memory
        void *more_memory = sbrk(EXPANSION_SIZE);

        if(more_memory == (void*)-1) return NULL;

        // Set up new block
        struct block *additional_block = (struct block*)more_memory;

        additional_block->size = EXPANSION_SIZE - sizeof(struct block);
        additional_block->is_free = 1;
        additional_block->next = NULL;

        // Add additional block at the end of the list
        if(heap == NULL){
            heap = additional_block;
        } else {
            struct block *last = heap;
            while(last->next != NULL){
                last = last->next;
            }
            last->next = additional_block;
        }

        current = additional_block;
    }
}
