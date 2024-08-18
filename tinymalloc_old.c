#include "tinymalloc_old.h"
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

#define SMALL_ALLOCATION_THRESHOLD (4 * BLOCK_SIZE)   // 64 bytes
#define LARGE_ALLOCATION_THRESHOLD (256 * BLOCK_SIZE) // 4096 bytes

typedef struct {
  uint8_t *heap;
  uint64_t *bitmap;
  size_t heap_size;
  size_t bitmap_size;
} BitmapAllocator_Old;

static BitmapAllocator_Old allocator_old;

static bool allocator_initialized_old = false;

pthread_mutex_t malloc_mutex_old = PTHREAD_MUTEX_INITIALIZER;

bool init_allocator_old() {
  allocator_old.heap_size = HEAP_SIZE;
  allocator_old.heap =
      mmap(NULL, allocator_old.heap_size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (allocator_old.heap == MAP_FAILED) {
    return false;
  }

  allocator_old.bitmap_size = (allocator_old.heap_size / BLOCK_SIZE + 63) / 64;
  allocator_old.bitmap =
      mmap(NULL, allocator_old.bitmap_size * sizeof(uint64_t),
           PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (allocator_old.bitmap == MAP_FAILED) {
    munmap(allocator_old.heap, allocator_old.heap_size);
    return false;
  }

  memset(allocator_old.bitmap, 0, allocator_old.bitmap_size);

  return true;
}

void set_bit_old(size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  allocator_old.bitmap[bitmap_index] |= (1ULL << bit_offset);
}

void clear_bit_old(size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  allocator_old.bitmap[bitmap_index] &= ~(1ULL << bit_offset);
}

bool is_bit_set_old(size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  return (allocator_old.bitmap[bitmap_index] & (1ULL << bit_offset)) != 0;
}

void *extend_heap_old(size_t size) {
  size_t page_size = sysconf(_SC_PAGESIZE);
  size_t extension_size = ((size + page_size - 1) / page_size) * page_size;

  size_t new_heap_size = allocator_old.heap_size + extension_size;
  size_t new_bitmap_size = (new_heap_size / BLOCK_SIZE + 63) / 64;

  void *new_memory = mmap(NULL, extension_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (new_memory == MAP_FAILED) {
    return false;
  }

  void *new_heap = mmap(NULL, new_heap_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
  if (new_heap == MAP_FAILED) {
    return NULL;
  }

  memcpy(new_heap, allocator_old.heap, allocator_old.heap_size);

  void *new_bitmap =
      mmap(NULL, new_bitmap_size * sizeof(uint64_t), PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANON, -1, 0);
  if (new_bitmap == MAP_FAILED) {
    munmap(new_heap, new_heap_size);
    return NULL;
  }

  memcpy(new_bitmap, allocator_old.bitmap,
         allocator_old.bitmap_size * sizeof(uint64_t));

  memset(new_bitmap + allocator_old.bitmap_size, 0,
         (new_bitmap_size - allocator_old.bitmap_size) * sizeof(uint64_t));

  munmap(allocator_old.heap, allocator_old.heap_size);
  munmap(allocator_old.bitmap, allocator_old.bitmap_size * sizeof(uint64_t));

  allocator_old.heap = new_heap;
  allocator_old.bitmap = new_bitmap;
  allocator_old.heap_size = new_heap_size;
  allocator_old.bitmap_size = new_bitmap_size;

  return (void *)((char *)allocator_old.heap + allocator_old.heap_size -
                  extension_size);
}

void *tinymalloc_old(size_t size) {
  if (size == 0)
    return NULL;

  if (!allocator_initialized_old) {
    pthread_mutex_lock(&malloc_mutex_old);
    if (!allocator_initialized_old) {
      if (!init_allocator_old()) {
        pthread_mutex_unlock(&malloc_mutex_old);
        return NULL;
      }
      allocator_initialized_old = true;
    }
    pthread_mutex_unlock(&malloc_mutex_old);
  }

  pthread_mutex_lock(&malloc_mutex_old);

  size_t blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  size_t bitmap_index = 0;

  size_t total_size = size + sizeof(size_t);

  size_t consecutive_free_blocks = 0;
  size_t allocation_start = 0;

  DEBUG_PRINT("Allocating %zu bytes\n", size);

  while (bitmap_index < allocator_old.bitmap_size) {
    if (allocator_old.bitmap[bitmap_index] != UINT64_MAX) {
      int first_free_bit;

      if (size <= SMALL_ALLOCATION_THRESHOLD ||
          size >= LARGE_ALLOCATION_THRESHOLD) {
        uint64_t bitmap_word = ~allocator_old.bitmap[bitmap_index];
        first_free_bit = 0;
        while (!(bitmap_word & 1)) {
          bitmap_word >>= 1;
          first_free_bit++;
        }
      } else {
        first_free_bit =
            __builtin_ffsll(~allocator_old.bitmap[bitmap_index]) - 1;
      }

      size_t start_block = bitmap_index * 64 + first_free_bit;
      size_t end_block = start_block + blocks_needed;

      if (end_block <= allocator_old.heap_size / BLOCK_SIZE) {
        bool enough_space = true;
        for (size_t i = start_block; i < end_block; i++) {
          if (is_bit_set_old(i)) {
            enough_space = false;
            break;
          }
        }

        if (enough_space) {
          for (size_t i = start_block; i < end_block; i++) {
            set_bit_old(i);
          }

          void *allocated_memory =
              allocator_old.heap + (start_block * BLOCK_SIZE);
          if (allocated_memory == NULL) {
            return NULL;
          }

          allocated_memory =
              (void *)(((uintptr_t)allocated_memory + sizeof(size_t) - 1) &
                       ~(sizeof(size_t) - 1));

          *(size_t *)allocated_memory = size;

          pthread_mutex_unlock(&malloc_mutex_old);

          return (char *)allocated_memory + sizeof(size_t);
        }
      }
    }

    bitmap_index++;
  }

  size_t extension_size =
      (blocks_needed * BLOCK_SIZE > allocator_old.heap_size / 4)
          ? (blocks_needed * BLOCK_SIZE)
          : (allocator_old.heap_size / 4);
  if (extend_heap_old(extension_size) != NULL) {
    size_t new_bitmap_size = allocator_old.bitmap_size;
    size_t start_bitmap_index =
        (allocator_old.heap_size - extension_size) / (BLOCK_SIZE * 64);

    for (size_t bitmap_index = start_bitmap_index;
         bitmap_index < new_bitmap_size; bitmap_index++) {
      if (allocator_old.bitmap[bitmap_index] != UINT64_MAX) {
        int first_free_bit;
        if (size <= SMALL_ALLOCATION_THRESHOLD ||
            size >= LARGE_ALLOCATION_THRESHOLD) {
          uint64_t bitmap_word = ~allocator_old.bitmap[bitmap_index];
          first_free_bit = 0;
          while (!(bitmap_word & 1)) {
            bitmap_word >>= 1;
            first_free_bit++;
          }
        } else {
          first_free_bit =
              __builtin_ffsll(~allocator_old.bitmap[bitmap_index]) - 1;
        }

        size_t start_block = bitmap_index * 64 + first_free_bit;
        size_t end_block = start_block + blocks_needed;

        if (end_block <= allocator_old.heap_size / BLOCK_SIZE) {
          bool enough_space = true;
          for (size_t i = start_block; i < end_block; i++) {
            if (is_bit_set_old(i)) {
              enough_space = false;
              break;
            }
          }

          if (enough_space) {
            for (size_t i = start_block; i < end_block; i++) {
              set_bit_old(i);
            }

            void *allocated_memory =
                allocator_old.heap + (start_block * BLOCK_SIZE);
            if (allocated_memory == NULL) {
              pthread_mutex_unlock(&malloc_mutex_old);
              return NULL;
            }

            allocated_memory =
                (void *)(((uintptr_t)allocated_memory + sizeof(size_t) - 1) &
                         ~(sizeof(size_t) - 1));
            *(size_t *)allocated_memory = size;

            pthread_mutex_unlock(&malloc_mutex_old);
            return (char *)allocated_memory + sizeof(size_t);
          }
        }
      }
    }
  }
  pthread_mutex_unlock(&malloc_mutex_old);
  return NULL;
}

void tinyfree_old(void *ptr) {
  if (ptr == NULL)
    return;

  pthread_mutex_lock(&malloc_mutex_old);

  void *actual_start = (char *)ptr - sizeof(size_t);

  if ((uintptr_t)actual_start < (uintptr_t)allocator_old.heap ||
      (uintptr_t)actual_start >=
          (uintptr_t)(allocator_old.heap + allocator_old.heap_size)) {
    pthread_mutex_unlock(&malloc_mutex_old);
    return;
  }

  size_t size = *(size_t *)actual_start;

  size_t block_index =
      ((uint8_t *)actual_start - allocator_old.heap) / BLOCK_SIZE;

  size_t blocks_to_free = (size + sizeof(size_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;

  if (block_index + blocks_to_free > allocator_old.heap_size / BLOCK_SIZE) {
    pthread_mutex_unlock(&malloc_mutex_old);
    return;
  }

  DEBUG_PRINT("Freeing memory at %p, size: %zu, block index: %zu, blocks to "
              "free: %zu\n",
              ptr, size, block_index, blocks_to_free);

  for (size_t i = block_index; i < block_index + blocks_to_free; i++) {
    clear_bit_old(i);
    DEBUG_PRINT("Cleared bit at index %zu\n", i);
  }

  pthread_mutex_unlock(&malloc_mutex_old);
}
