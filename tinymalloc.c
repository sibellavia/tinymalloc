/*
 * tinymalloc - A simple memory allocator
 * Copyright (C) 2023 @sibellavia
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#define _GNU_SOURCE
#include <sched.h>

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

#ifndef HAVE_SCHED_GETCPU
#include <sys/syscall.h>
#include <unistd.h>

int sched_getcpu(void) {
  int cpu;
#ifdef SYS_getcpu
  if (syscall(SYS_getcpu, &cpu, NULL, NULL) == -1) {
    return -1;
  }
  return cpu;
#else
  return -1;
#endif
}
#endif

// basic constants
#define HEAP_SIZE 1048576 // 1 mb
#define BLOCK_SIZE 16     // 16 bytes per block

// derived constants
#define BITMAP_SIZE (HEAP_SIZE / BLOCK_SIZE / 64)     // size of bitmap in bytes
#define SMALL_ALLOCATION_THRESHOLD (4 * BLOCK_SIZE)   // 64 bytes
#define LARGE_ALLOCATION_THRESHOLD (256 * BLOCK_SIZE) // 4096 bytes

// compile-time assertions
_Static_assert(HEAP_SIZE % BLOCK_SIZE == 0,
               "HEAP_SIZE must be a multiple of BLOCK_SIZE");
_Static_assert(BITMAP_SIZE > 0, "BITMAP_SIZE must be greater than 0");

typedef struct {
  BitmapAllocator allocator;
  pthread_mutex_t mutex;
} Arena;

static Arena *arenas = NULL;
static int num_arenas = 0;
static bool arenas_initialized = false;
static bool allocator_initialized = false;
pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

int get_num_cpus() { return sysconf(_SC_NPROCESSORS_ONLN); }

static void cleanup_allocator(BitmapAllocator *allocator) {
  if (allocator->heap != NULL) {
    munmap(allocator->heap, allocator->heap_size);
    allocator->heap = NULL;
  }
  if (allocator->bitmap != NULL) {
    munmap(allocator->bitmap, allocator->bitmap_size * sizeof(uint64_t));
    allocator->bitmap = NULL;
  }
}

static void cleanup_arenas() {
  if (arenas != NULL) {
    for (int i = 0; i < num_arenas; i++) {
      cleanup_allocator(&arenas[i].allocator);
    }
    munmap(arenas, num_arenas * sizeof(Arena));
    arenas = NULL;
  }
  num_arenas = 0;
  arenas_initialized = false;
}

/* init_allocator() */
// here mmap is used to allocate a large chunk of memory for the heap
// after the check, mmset is used to initialize our bitmap to all zeros
static bool init_allocator(BitmapAllocator *allocator) {
  // allocate memory from the heap
  allocator->heap_size = HEAP_SIZE;
  allocator->heap = mmap(NULL, allocator->heap_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (allocator->heap == MAP_FAILED) {
    return false; // initialization failed
  }

  // initialize the bitmap (all blocks are free)
  allocator->bitmap_size = (allocator->heap_size / BLOCK_SIZE + 63) / 64;
  allocator->bitmap =
      mmap(NULL, allocator->bitmap_size * sizeof(uint64_t),
           PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (allocator->bitmap == MAP_FAILED) {
    munmap(allocator->heap, allocator->heap_size);
    return false; // initialization failed
  }

  // initialize bitmap to zero (all blocks are free)
  memset(allocator->bitmap, 0, allocator->bitmap_size * sizeof(uint64_t));

  return true; // initialization succeeded
}

static bool init_arenas() {
  num_arenas = get_num_cpus();

  arenas = mmap(NULL, num_arenas * sizeof(Arena), PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (arenas == MAP_FAILED) {
    return false;
  }

  for (int i = 0; i < num_arenas; i++) {
    if (!init_allocator(&arenas[i].allocator)) {
      // clean up previously initialized arenas
      for (int j = 0; j < i; j++) {
        cleanup_allocator(&arenas[j].allocator);
      }
      if (pthread_mutex_init(&arenas[i].mutex, NULL) != 0) {
        cleanup_allocator(&arenas[i].allocator);
        return false;
      }
      munmap(arenas, num_arenas * sizeof(Arena));
      arenas = NULL;
      num_arenas = 0;
      return false;
    }
  }

  return true;
}

/* set_bit(size_t index) */
// sets a specific bit to 1 (marking a block as used)
static inline void set_bit(BitmapAllocator *allocator, size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  allocator->bitmap[bitmap_index] |= (1ULL << bit_offset);
}

/* clear_bit(size_t index) */
// sets a specific bit to 0 (marking a block as free)
static inline void clear_bit(BitmapAllocator *allocator, size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  allocator->bitmap[bitmap_index] &= ~(1ULL << bit_offset);
}

/* is_bit_set(size_t index) */
// checks if a specific bit is 1 or 0
static inline bool is_bit_set(BitmapAllocator *allocator, size_t index) {
  size_t bitmap_index = index / 64;
  size_t bit_offset = index % 64;
  return (allocator->bitmap[bitmap_index] & (1ULL << bit_offset)) != 0;
}

static void *extend_heap(BitmapAllocator *allocator, size_t size) {
  size_t page_size = sysconf(_SC_PAGESIZE);
  // round up size to a multiple of HEAP_SIZE
  size_t extension_size = ((size + page_size - 1) / page_size) * page_size;

  // calculate new sizes
  size_t new_heap_size = allocator->heap_size + extension_size;
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
  memcpy(new_heap, allocator->heap, allocator->heap_size);

  // allocate new bitmap
  void *new_bitmap =
      mmap(NULL, new_bitmap_size * sizeof(uint64_t), PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANON, -1, 0);
  if (new_bitmap == MAP_FAILED) {
    munmap(new_heap, new_heap_size);
    return NULL;
  }

  // copy old bitmap to new location
  memcpy(new_bitmap, allocator->bitmap,
         allocator->bitmap_size * sizeof(uint64_t));

  // initialize new bitmap area to 0 (all blocks free)
  memset(new_bitmap + allocator->bitmap_size, 0,
         (new_bitmap_size - allocator->bitmap_size) * sizeof(uint64_t));

  // free old heap and bitmap
  munmap(allocator->heap, allocator->heap_size);
  munmap(allocator->bitmap, allocator->bitmap_size * sizeof(uint64_t));

  // update allocator structure and finally we return!
  allocator->heap = new_heap;
  allocator->bitmap = new_bitmap;
  allocator->heap_size = new_heap_size;
  allocator->bitmap_size = new_bitmap_size;

  return (void *)((char *)allocator->heap + allocator->heap_size -
                  extension_size);
}

static size_t find_free_blocks(BitmapAllocator *allocator,
                               size_t blocks_needed) {
  for (size_t bitmap_index = 0;
       bitmap_index < allocator->bitmap_size / sizeof(uint64_t);
       bitmap_index++) {
    if (allocator->bitmap[bitmap_index] != UINT64_MAX) {
      int first_free_bit;
      if (blocks_needed <= SMALL_ALLOCATION_THRESHOLD / BLOCK_SIZE ||
          blocks_needed >= LARGE_ALLOCATION_THRESHOLD / BLOCK_SIZE) {
        // hybrid approach here
        // manual bit scanning for small and large allocations
        uint64_t bitmap_word = ~allocator->bitmap[bitmap_index];
        first_free_bit = __builtin_ctzll(bitmap_word);
      } else {
        // __builtin_ffsll for medium allocations
        first_free_bit = __builtin_ffsll(~allocator->bitmap[bitmap_index]) - 1;
      }

      size_t start_block = bitmap_index * 64 + first_free_bit;
      size_t end_block = start_block + blocks_needed;

      if (end_block <= allocator->heap_size / BLOCK_SIZE) {
        bool enough_space = true;
        for (size_t i = start_block; i < end_block; i++) {
          if (is_bit_set(allocator, i)) {
            enough_space = false;
            break;
          }
        }
        if (enough_space) {
          return start_block;
        }
      }
    }
  }
  return SIZE_MAX; // not found :-(
}

static void *allocate_blocks(BitmapAllocator *allocator, size_t start_block,
                             size_t blocks_needed, size_t size) {
  for (size_t i = start_block; i < start_block + blocks_needed; i++) {
    set_bit(allocator, i);
  }

  void *allocated_memory = allocator->heap + (start_block * BLOCK_SIZE);
  allocated_memory =
      (void *)(((uintptr_t)allocated_memory + sizeof(size_t) - 1) &
               ~(sizeof(size_t) - 1));
  *(size_t *)allocated_memory = size;

  return (char *)allocated_memory + sizeof(size_t);
}

static Arena *get_thread_arena() {
  int cpu = sched_getcpu();
  if (cpu < 0 || cpu >= num_arenas) {
    // sched_getcpu() failed or returned an out-of-range value, fallback to a
    // default behavior
    cpu = 0;
  }
  return &arenas[cpu];
}

/* tinymalloc */
void *tinymalloc(size_t size) {
  if (size == 0)
    return NULL;

  pthread_mutex_lock(&malloc_mutex);

  if (!arenas_initialized) {
    if (!init_arenas()) {
      pthread_mutex_unlock(&malloc_mutex);
      return NULL;
    }
    arenas_initialized = true;
  }

  Arena *arena = get_thread_arena();
  BitmapAllocator *allocator = &arena->allocator;

  size_t total_size = size + sizeof(size_t);
  size_t blocks_needed = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  DEBUG_PRINT("allocating %zu bytes (%zu blocks)\n", size, blocks_needed);

  size_t start_block = find_free_blocks(allocator, blocks_needed);

  if (start_block == SIZE_MAX) {
    // couldn't find space, i try to extend heap
    size_t extension_size =
        (blocks_needed * BLOCK_SIZE > allocator->heap_size / 4)
            ? (blocks_needed * BLOCK_SIZE)
            : (allocator->heap_size / 4);
    if (extend_heap(allocator, extension_size) == NULL) {
      pthread_mutex_unlock(&malloc_mutex);
      return NULL;
    }

    // try allocation again...
    start_block = find_free_blocks(allocator, blocks_needed);
    if (start_block == SIZE_MAX) {
      pthread_mutex_unlock(&malloc_mutex);
      return NULL;
    }
  }

  void *result = allocate_blocks(allocator, start_block, blocks_needed, size);

  pthread_mutex_unlock(&malloc_mutex);
  return result;
}

/* tinyfree */
void tinyfree(void *ptr) {
  if (ptr == NULL)
    return;

  pthread_mutex_lock(&malloc_mutex);

  // find the arena that this pointer belongs to
  Arena *arena = NULL;
  for (int i = 0; i < num_arenas; i++) {
    if ((uintptr_t)ptr >= (uintptr_t)arenas[i].allocator.heap &&
        (uintptr_t)ptr < (uintptr_t)(arenas[i].allocator.heap +
                                     arenas[i].allocator.heap_size)) {
      arena = &arenas[i];
      break;
    }
  }

  if (arena == NULL) {
    pthread_mutex_unlock(&malloc_mutex);
    return; // Invalid pointer
  }

  BitmapAllocator *allocator = &arena->allocator;

  // calculate which block this pointer corrisponds to
  void *actual_start = (char *)ptr - sizeof(size_t);

  if ((uintptr_t)actual_start < (uintptr_t)allocator->heap ||
      (uintptr_t)actual_start >=
          (uintptr_t)(allocator->heap + allocator->heap_size)) {
    pthread_mutex_unlock(&malloc_mutex);
    return;
  }

  // retrieve the size of the allocation
  size_t size = *(size_t *)actual_start;

  // calculate which block this pointer corresponds to
  size_t block_index = ((uint8_t *)actual_start - allocator->heap) / BLOCK_SIZE;

  // calculate how many blocks were allocated
  size_t blocks_to_free = (size + sizeof(size_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;

  if (block_index + blocks_to_free > allocator->heap_size / BLOCK_SIZE) {
    pthread_mutex_unlock(&malloc_mutex);
    return;
  }

  DEBUG_PRINT("freeing memory at %p, size: %zu, block index: %zu, blocks to "
              "free: %zu\n",
              ptr, size, block_index, blocks_to_free);

  // mark the blocks as free
  for (size_t i = block_index; i < block_index + blocks_to_free; i++) {
    clear_bit(allocator, i);
    DEBUG_PRINT("cleared bit at index %zu\n", i);
  }

  pthread_mutex_unlock(&malloc_mutex);
}
