#ifndef TINYMALLOC_H
#define TINYMALLOC_H

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

// constants
#define HEAP_SIZE 1048576
#define BLOCK_SIZE 16      
#define BITMAP_SIZE (HEAP_SIZE / BLOCK_SIZE / 8)  

// // BitmapAllocator struct
// typedef struct {
//     uint8_t *heap;
//     uint8_t bitmap[BITMAP_SIZE];
// } BitmapAllocator;

// function prototypes
void *tinymalloc(size_t size);
void tinyfree(void *ptr);

// initialization function
bool init_allocator();

// bitmap manipulation functions
void set_bit(size_t index);
void clear_bit(size_t index);
bool is_bit_set(size_t index);

// helper functions
size_t calculate_blocks_needed(size_t size);
void *allocate_blocks(size_t start_block, size_t blocks_needed);

#endif 