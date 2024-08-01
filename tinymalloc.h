#ifndef TINYMALLOC_H
#define TINYMALLOC_H

#include <stddef.h>

// Function prototypes
void *tinymalloc(size_t size);
void tinyfree(void *ptr);

void *initialize_heap();
struct block_header *find_free_block(size_t size);
struct block_header *extend_heap(size_t size);
struct block_header *split_block(struct block_header *block, size_t size);
void remove_from_free_list(struct block_header *block);
void coalesce(struct block_header *block);

#endif
