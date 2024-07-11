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
    if (size == 0) return NULL;

    // Initialize Heap if it hasn't been initialized yet
    if(heap_start == NULL){
        void *initial_mem = sbrk(INITIAL_HEAP_SIZE);
        if(initial_mem == (void*)-1){
            // When sbrk() fails to allocate memory, it returns (void*)-1
            // If so, return NULL as an error
            return NULL;
        }

        // sbrk() returns a void pointer to the start of the newly allocated memory.
        // We cast this to struct block* because we want to use the begininng of this
        // memory to store our block metadata.
        heap_start = (struct block*)initial_mem;

        heap_start->size = INITIAL_HEAP_SIZE - sizeof(struct block);
        heap_start->is_free = 1;
        heap_start->next = NULL;
    }

    struct block *current = heap_start;

    while(1){
        while(current){
            if(current->is_free && current->size >= size){
                // We found a suitable block
                current->is_free = 0;

                // Split if size is too much
                if(current->size > size + sizeof(struct block) + 16){
                    struct block *new_block = (struct block*)((char*)current + sizeof(struct block) + size);
                    new_block->size = current->size - size - sizeof(struct block);
                    new_block->is_free = 1;
                    new_block->next = current->next;
                    current->size = size;
                    current->next = new_block;
                }
                return (void*)(current + 1); // We return pointer to usable memory
            }
            current = current->next;
        }

        // No suitable block found, we request more memory
        void *more_mem = sbrk(EXPANSION_SIZE);

        if(more_mem == (void*)-1){
            // When sbrk() fails to allocate memory, it returns (void*)-1
            // If so, return NULL as an error
            return NULL;
        }

        // Set up new block
        struct block *new_block = (struct block*)more_mem;

        new_block->size = EXPANSION_SIZE - sizeof(struct block);
        new_block->is_free = 1;
        new_block->next = NULL;

        // Add new block at the end of the list
        if(heap_start == NULL){
            heap_start = new_block;
        } else {
            struct block *last = heap_start;
            while(last->next != NULL){
                last = last->next;
            }
            last->next = new_block;
        }

        current = new_block;
    }
}
