#include "tinymalloc.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

#define HEAP_SIZE 1048576                         // 1 mb
#define BLOCK_SIZE 16                             // 16 bytes per block
#define BITMAP_SIZE (HEAP_SIZE / BLOCK_SIZE / 64) // size of bitmap in bytes

#define SMALL_ALLOCATION_THRESHOLD  (4 * BLOCK_SIZE)   // 64 bytes
#define LARGE_ALLOCATION_THRESHOLD  (256 * BLOCK_SIZE) // 4096 bytes

/* Bitmap */
// uint8_t *heap: this is a pointer to the memory we'll allocate from.
// uint8_t bitmap[BITMAP_SIZE]: this array represents which blocks of
// memory are free or in use. each bit corresponds to one block of
// memory in the heap.
typedef struct {
  uint8_t *heap;
  uint64_t *bitmap;
  size_t heap_size;
  size_t bitmap_size;
} BitmapAllocator;

static BitmapAllocator allocator;

static bool allocator_initialized = false;

pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/* init_allocator() */
// here mmap is used to allocate a large chunk of memory for the heap
// after the check, mmset is used to initialize our bitmap to all zeros
bool init_allocator() {
  // allocate memory from the heap
  allocator.heap_size = HEAP_SIZE;
  allocator.heap = mmap(NULL, allocator.heap_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (allocator.heap == MAP_FAILED) {
    return false; // initialization failed
  }

  // initialize the bitmap (all blocks are free)
  allocator.bitmap_size = (allocator.heap_size / BLOCK_SIZE + 63) / 64;
  allocator.bitmap =
      mmap(NULL, allocator.bitmap_size * sizeof(uint64_t),
           PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (allocator.bitmap == MAP_FAILED) {
    munmap(allocator.heap, allocator.heap_size);
    return false; // initialization failed
  }

  // initialize bitmap to zero (all blocks are free)
  memset(allocator.bitmap, 0, allocator.bitmap_size);

  return true; // initialization succeeded
}

/* set_bit(size_t index) */
// sets a specific bit to 1 (marking a block as used)
void set_bit(size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  allocator.bitmap[bitmap_index] |= (1ULL << bit_offset);
}

/* clear_bit(size_t index) */
// sets a specific bit to 0 (marking a block as free)
void clear_bit(size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  allocator.bitmap[bitmap_index] &= ~(1ULL << bit_offset);
}

/* is_bit_set(size_t index) */
// checks if a specific bit is 1 or 0
bool is_bit_set(size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  return (allocator.bitmap[bitmap_index] & (1ULL << bit_offset)) != 0;
}

void *extend_heap(size_t size) {
  size_t page_size = sysconf(_SC_PAGESIZE);
  // round up size to a multiple of HEAP_SIZE
  size_t extension_size = ((size + page_size - 1) / page_size) * page_size;

  // calculate new sizes
  size_t new_heap_size = allocator.heap_size + extension_size;
  size_t new_bitmap_size = (new_heap_size / BLOCK_SIZE + 63) /
                           64; // i round up to nearest 64-bit unit

  // mmap goes brr
  void *new_memory = mmap(NULL, extension_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (new_memory == MAP_FAILED) {
    return false; // request memory from system failed
  }

  // allocate new heap
  void *new_heap = mmap(NULL, new_heap_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
  if (new_heap == MAP_FAILED) {
    return NULL;
  }

  // copy old heap to new location
  memcpy(new_heap, allocator.heap, allocator.heap_size);

  // allocate new bitmap
  void *new_bitmap =
      mmap(NULL, new_bitmap_size * sizeof(uint64_t), PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANON, -1, 0);
  if (new_bitmap == MAP_FAILED) {
    munmap(new_heap, new_heap_size);
    return NULL;
  }

  // copy old bitmap to new location
  memcpy(new_bitmap, allocator.bitmap,
         allocator.bitmap_size * sizeof(uint64_t));

  // initialize new bitmap area to 0 (all blocks free)
  memset(new_bitmap + allocator.bitmap_size, 0,
         (new_bitmap_size - allocator.bitmap_size) * sizeof(uint64_t));

  // free old heap and bitmap
  munmap(allocator.heap, allocator.heap_size);
  munmap(allocator.bitmap, allocator.bitmap_size * sizeof(uint64_t));

  // update allocator structure and finally we return!
  allocator.heap = new_heap;
  allocator.bitmap = new_bitmap;
  allocator.heap_size = new_heap_size;
  allocator.bitmap_size = new_bitmap_size;

  return (void *)((char *)allocator.heap + allocator.heap_size -
                  extension_size);
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
  size_t bitmap_index = 0;

  // add space for size storage
  size_t total_size = size + sizeof(size_t);

  // first-fit search in the bitmap
  size_t consecutive_free_blocks = 0;
  size_t allocation_start = 0;

  DEBUG_PRINT("Allocating %zu bytes\n", size);

  while (bitmap_index < allocator.bitmap_size) {
    if (allocator.bitmap[bitmap_index] != UINT64_MAX) {
      // not all bits are set
      // i try to find the first free bit
      int first_free_bit;

      // i try an hybrid approach here
      // manual bit scanning for small and large allocations
      // __builtin_ffsll for medium allocations
      if (size <= SMALL_ALLOCATION_THRESHOLD || size >= LARGE_ALLOCATION_THRESHOLD){
        // mnual bit scanning for small and large allocations
        uint64_t bitmap_word = ~allocator.bitmap[bitmap_index];
        first_free_bit = 0;
        while (!(bitmap_word & 1)){
          bitmap_word >>= 1;
          first_free_bit++;
        }
      } else {
        // use __builtin_ffsll for medium allocations
        first_free_bit = __builtin_ffsll(~allocator.bitmap[bitmap_index]) - 1;
      }

      // check if there are enough contiguous free blocks
      size_t start_block = bitmap_index * 64 + first_free_bit;
      size_t end_block = start_block + blocks_needed;

      if (end_block <= allocator.heap_size / BLOCK_SIZE) {
        bool enough_space = true;
        for (size_t i = start_block; i < end_block; i++) {
          if (is_bit_set(i)) {
            enough_space = false;
            break;
          }
        }

        if (enough_space) {
          // allocate the blocks
          for (size_t i = start_block; i < end_block; i++) {
            set_bit(i);
          }

          // calculate the memory address to return
          // pointer arithmetic: i'm multiplying starting_block by BLOCK_SIZE
          // to get the byte offset, and adding it to the heap
          void *allocated_memory = allocator.heap + (start_block * BLOCK_SIZE);
          if (allocated_memory == NULL) {
            return NULL; // allocation failed
          }

          // ensure alignment
          allocated_memory =
              (void *)(((uintptr_t)allocated_memory + sizeof(size_t) - 1) &
                       ~(sizeof(size_t) - 1));

          // store the requested size at the beginning of the allocated memory
          *(size_t *)allocated_memory = size;

          pthread_mutex_unlock(&malloc_mutex);

          // return the address after the size storage
          return (char *)allocated_memory + sizeof(size_t);
        }
      }
    }

    bitmap_index++;
  }

  // if we get here, we couldn't find enough space
  // we try to extend the heap
  size_t extension_size = (blocks_needed * BLOCK_SIZE > allocator.heap_size / 4)
                              ? (blocks_needed * BLOCK_SIZE)
                              : (allocator.heap_size / 4);
  if (extend_heap(extension_size) != NULL) {
    // heap extended successfully, we retry the allocation
    size_t new_bitmap_size = allocator.bitmap_size;
    size_t start_bitmap_index =
        (allocator.heap_size - extension_size) / (BLOCK_SIZE * 64);

    // start scanning from where the new memory was added
    for (size_t bitmap_index = start_bitmap_index;
         bitmap_index < new_bitmap_size; bitmap_index++){
      if (allocator.bitmap[bitmap_index] != UINT64_MAX){
        int first_free_bit;
        if (size <= SMALL_ALLOCATION_THRESHOLD || size >= LARGE_ALLOCATION_THRESHOLD){
          // manual bit scanning for small and large allocations
          uint64_t bitmap_word = ~allocator.bitmap[bitmap_index];
          first_free_bit = 0;
          while (!(bitmap_word & 1)){
            bitmap_word >>= 1;
            first_free_bit++;
          }
        } else {
          // use __builtin_ffsll for medium allocations
          first_free_bit = __builtin_ffsll(~allocator.bitmap[bitmap_index]) - 1;
        }

        size_t start_block = bitmap_index * 64 + first_free_bit;
        size_t end_block = start_block + blocks_needed;

        if (end_block <= allocator.heap_size / BLOCK_SIZE) {
          bool enough_space = true;
          for (size_t i = start_block; i < end_block; i++) {
            if (is_bit_set(i)) {
              enough_space = false;
              break;
            }
          }

          if (enough_space) {
            // allocate the blocks
            for (size_t i = start_block; i < end_block; i++) {
              set_bit(i);
            }

            void *allocated_memory =
                allocator.heap + (start_block * BLOCK_SIZE);
            if (allocated_memory == NULL) {
              pthread_mutex_unlock(&malloc_mutex);
              return NULL; // allocation failed
            }

            // ensure alignment
            allocated_memory =
                (void *)(((uintptr_t)allocated_memory + sizeof(size_t) - 1) &
                         ~(sizeof(size_t) - 1));
            // store the requested size at the beginning of the allocated memory
            *(size_t *)allocated_memory = size;

            pthread_mutex_unlock(&malloc_mutex);
            // return the address after the size storage
            return (char *)allocated_memory + sizeof(size_t);
          }
        }
      }
    }
  }
  // if we still couldn't allocate after extending, it's a fail
  pthread_mutex_unlock(&malloc_mutex);
  return NULL;
}

/* tinyfree */
void tinyfree(void *ptr) {
  if (ptr == NULL)
    return;

  pthread_mutex_lock(&malloc_mutex);

  // calculate which block this pointer corrisponds to
  void *actual_start = (char *)ptr - sizeof(size_t);

  if ((uintptr_t)actual_start < (uintptr_t)allocator.heap ||
      (uintptr_t)actual_start >=
          (uintptr_t)(allocator.heap + allocator.heap_size)) {
    pthread_mutex_unlock(&malloc_mutex);
    return;
  }

  // retrieve the size of the allocation
  size_t size = *(size_t *)actual_start;

  // calculate which block this pointer corresponds to
  size_t block_index = ((uint8_t *)actual_start - allocator.heap) / BLOCK_SIZE;

  // calculate how many blocks were allocated
  size_t blocks_to_free = (size + sizeof(size_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;

  if (block_index + blocks_to_free > allocator.heap_size / BLOCK_SIZE) {
    pthread_mutex_unlock(&malloc_mutex);
    return;
  }

  DEBUG_PRINT("Freeing memory at %p, size: %zu, block index: %zu, blocks to "
              "free: %zu\n",
              ptr, size, block_index, blocks_to_free);

  // mark the blocks as free
  for (size_t i = block_index; i < block_index + blocks_to_free; i++) {
    clear_bit(i);
    DEBUG_PRINT("Cleared bit at index %zu\n", i);
  }

  pthread_mutex_unlock(&malloc_mutex);
}
