#include "tinymalloc.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define HEAP_SIZE 1048576                        // 1 mb
#define BLOCK_SIZE 16                            // 16 bytes per block
#define BITMAP_SIZE (HEAP_SIZE / BLOCK_SIZE / 8) // size of bitmap in bytes
#define ALIGNMENT _Alignof(max_align_t)

/* Block Structure */
typedef struct memory_block {
  size_t size;
  struct memory_block *next;
  struct memory_block *prev;
  int is_free;
} memory_block_t;

/* Bitmap */
// uint8_t *heap: this is a pointer to the memory we'll allocate from.
// uint8_t bitmap[BITMAP_SIZE]: this array represents which blocks of
// memory are free or in use. each bit corresponds to one block of
// memory in the heap.
typedef struct {
  uint8_t *heap;
  uint8_t bitmap[BITMAP_SIZE];
} BitmapAllocator;

static memory_block_t *heap_head = NULL;

static BitmapAllocator allocator;

static bool allocator_initialized = false;

pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/* init_allocator() */
// here mmap is used to allocate a large chunk of memory for the heap
// after the check, mmset is used to initialize our bitmap to all zeros
bool init_allocator() {
  // allocate memory from the heap
  allocator.heap = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (allocator.heap == MAP_FAILED) {
    return false; // initialization failed
  }

  // initialize the bitmap (all blocks are free)
  memset(allocator.bitmap, 0, BITMAP_SIZE);

  return true; // initialization succeeded
}

/* set_bit(size_t index) */
// sets a specific bit to 1 (marking a block as used)
void set_bit(size_t index) {
  allocator.bitmap[index / 8] |= (1 << (index % 8));
}

/* clear_bit(size_t index) */
// sets a specific bit to 0 (marking a block as free)
void clear_bit(size_t index) {
  allocator.bitmap[index / 8] &= ~(1 << (index % 8));
}

/* is_bit_set(size_t index) */
// checks if a specific bit is 1 or 0
bool is_bit_set(size_t index) {
  return (allocator.bitmap[index / 8] & (1 << (index % 8))) != 0;
}

/* new tinymalloc based on bitmap */
void *tinymalloc(size_t size) {
  if (size == 0)
    return NULL;

  if (!allocator_initialized) {
    pthread_mutex_lock(&malloc_mutex);
    if (!allocator_initialized) {
      if (!init_allocator()) {
        pthread_mutex_unlock(&malloc_mutex);
        return NULL;
      }
      allocator_initialized = true;
    }
    pthread_mutex_unlock(&malloc_mutex);
  }

  pthread_mutex_lock(&malloc_mutex);

  // calculate how many blocks we need
  size_t blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  // add space for size storage
  size_t total_size = size + sizeof(size_t);

  // first-fit search in the bitmap
  size_t consecutive_free_blocks = 0;
  size_t allocation_start = 0;

  for (size_t i = 0; i < HEAP_SIZE / BLOCK_SIZE; i++) {
    if (!is_bit_set(i)) {
      // this block is free
      consecutive_free_blocks++;
      if (consecutive_free_blocks == blocks_needed) {
        // we've found enough space
        // mark the blocks as used
        allocation_start =
            i - blocks_needed +
            1; // i is currently at the last block of our found sequence
        for (size_t j = 0; j < blocks_needed; j++) {
          set_bit(allocation_start + j);
        }

        // calculate the memory address to return
        // pointer arithmetic: i'm multiplying starting_block by BLOCK_SIZE
        // to get the byte offset, and adding it to the heap
        void *allocated_memory =
            allocator.heap + (allocation_start * BLOCK_SIZE);
        if (allocated_memory == NULL) {
          return NULL; // allocation failed
        }
        // store the requested size at the beginning of the allocated memory
        *(size_t *)allocated_memory = size;

        pthread_mutex_unlock(&malloc_mutex);

        // return the address after the size storage
        return (char *)allocated_memory + sizeof(size_t);
      }
    } else {
      // this block is used, reset our counter
      consecutive_free_blocks = 0;
    }
  }

  // if we get here, we couldn't find enough space
  // todo: extend heap
  pthread_mutex_unlock(&malloc_mutex);
  return NULL; // allocation failed
}

/* tinyfree */
void tinyfree(void *ptr) {
  if (ptr == NULL)
    return;

  pthread_mutex_lock(&malloc_mutex);

  // todo: calculate which block this pointer corrisponds to
  void *actual_start = (char *)ptr - sizeof(size_t);

  // retrieve the size of the allocation
  size_t size = *(size_t *)actual_start;

  // calculate which block this pointer corresponds to
  size_t block_index = ((uint8_t *)actual_start - allocator.heap) / BLOCK_SIZE;

  // calculate how many blocks were allocated
  size_t blocks_to_free = (size + sizeof(size_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;

  // mark the blocks as free
  for (size_t i = block_index; i < block_index + blocks_to_free; i++) {
    clear_bit(i);
  }

  pthread_mutex_unlock(&malloc_mutex);
}
