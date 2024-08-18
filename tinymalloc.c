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
#include <stdatomic.h>
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
  size_t allocated_blocks;
} Arena;

static Arena *arenas = NULL;
static int num_arenas = 0;
static bool arenas_initialized = false;
static int next_arena_index = 0;
pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t arena_index_mutex = PTHREAD_MUTEX_INITIALIZER;

int get_num_cpus() { return sysconf(_SC_NPROCESSORS_ONLN); }

static void update_arena_usage(Arena *arena, size_t blocks) {
  arena->allocated_blocks += blocks;
}

static void cleanup_memory() {
  if (arenas != NULL) {
    for (int i = 0; i < num_arenas; i++) {
      if (arenas[i].allocator.heap != NULL) {
        munmap(arenas[i].allocator.heap, arenas[i].allocator.heap_size);
      }
      if (arenas[i].allocator.bitmap != NULL) {
        munmap(arenas[i].allocator.bitmap,
               arenas[i].allocator.bitmap_size * sizeof(uint64_t));
      }
      pthread_mutex_destroy(&arenas[i].mutex);
    }
    munmap(arenas, num_arenas * sizeof(Arena));
    arenas = NULL;
  }
  num_arenas = 0;
  arenas_initialized = false;
}

static bool init_memory() {
  num_arenas = get_num_cpus();
  arenas = mmap(NULL, num_arenas * sizeof(Arena), PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (arenas == MAP_FAILED) {
    return false;
  }

  for (int i = 0; i < num_arenas; i++) {
    Arena *arena = &arenas[i];
    arena->allocator.heap_size = HEAP_SIZE;
    arena->allocator.heap =
        mmap(NULL, arena->allocator.heap_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena->allocator.heap == MAP_FAILED) {
      cleanup_memory();
      return false;
    }

    arena->allocator.bitmap_size =
        (arena->allocator.heap_size / BLOCK_SIZE + 63) / 64;
    arena->allocator.bitmap =
        mmap(NULL, arena->allocator.bitmap_size * sizeof(uint64_t),
             PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena->allocator.bitmap == MAP_FAILED) {
      cleanup_memory();
      return false;
    }

    memset(arena->allocator.bitmap, 0,
           arena->allocator.bitmap_size * sizeof(uint64_t));
    arena->allocated_blocks = 0;

    if (pthread_mutex_init(&arena->mutex, NULL) != 0) {
      cleanup_memory();
      return false;
    }
  }

  arenas_initialized = true;
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

static void free_blocks(BitmapAllocator *allocator, size_t start_index,
                        size_t num_blocks) {
  for (size_t i = 0; i < num_blocks; i++) {
    clear_bit(allocator, start_index + i);
  }
  allocator->allocated_blocks -= num_blocks;
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

  allocator->allocated_blocks += blocks_needed;

  void *allocated_memory = allocator->heap + (start_block * BLOCK_SIZE);
  allocated_memory =
      (void *)(((uintptr_t)allocated_memory + sizeof(size_t) - 1) &
               ~(sizeof(size_t) - 1));
  *(size_t *)allocated_memory = size;

  return (char *)allocated_memory + sizeof(size_t);
}

static Arena *select_arena(size_t size) {
  static _Thread_local int thread_arena_index = -1;

  // initialize thread_arena_index if it hasn't been set
  if (thread_arena_index == -1) {
    pthread_mutex_lock(&arena_index_mutex);
    thread_arena_index = next_arena_index;
    next_arena_index = (next_arena_index + 1) % num_arenas;
    pthread_mutex_unlock(&arena_index_mutex);
  }

  // for small allocations, use the thread's assigned arena
  if (size <= LARGE_ALLOCATION_THRESHOLD) {
    return &arenas[thread_arena_index];
  }

  // for large allocations, find the least used arena
  Arena *suitable_arena = &arenas[0];
  size_t min_usage = SIZE_MAX;

  for (int i = 0; i < num_arenas; i++) {
    size_t usage = arenas[i].allocated_blocks * BLOCK_SIZE;
    if (usage < min_usage && arenas[i].allocator.heap_size - usage >= size) {
      min_usage = usage;
      suitable_arena = &arenas[i];
    }
  }

  return suitable_arena;
}

static void *try_allocate(Arena *arena, size_t size) {
  BitmapAllocator *allocator = &arena->allocator;

  size_t total_size = size + sizeof(size_t);
  size_t blocks_needed = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  DEBUG_PRINT("allocating %zu bytes (%zu blocks)\n", size, blocks_needed);

  size_t start_block = find_free_blocks(allocator, blocks_needed);

  if (start_block == SIZE_MAX) {
    // couldn't find space, try to extend heap
    size_t extension_size =
        (blocks_needed * BLOCK_SIZE > allocator->heap_size / 4)
            ? (blocks_needed * BLOCK_SIZE)
            : (allocator->heap_size / 4);
    if (extend_heap(allocator, extension_size) == NULL) {
      return NULL;
    }

    // try allocation again
    start_block = find_free_blocks(allocator, blocks_needed);
    if (start_block == SIZE_MAX) {
      return NULL;
    }
  }

  return allocate_blocks(allocator, start_block, blocks_needed, size);
}

static bool initialize_if_needed() {
  if (!arenas_initialized) {
    if (!init_memory()) {
      return false;
    }
    arenas_initialized = true;
  }
  return true;
}

static void *allocate_memory(size_t size) {
  Arena *arena = select_arena(size);
  void *result = try_allocate(arena, size);

  if (result != NULL) {
    size_t blocks_allocated =
        (size + sizeof(size_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    update_arena_usage(arena, blocks_allocated);
  }

  return result;
}

/* tinymalloc */
void *tinymalloc(size_t size) {
  if (size == 0)
    return NULL;

  pthread_mutex_lock(&malloc_mutex);

  if (!initialize_if_needed()) {
    pthread_mutex_unlock(&malloc_mutex);
    return NULL;
  }

  void *result = allocate_memory(size);

  pthread_mutex_unlock(&malloc_mutex);
  return result;
}

static Arena *find_arena_for_pointer(void *ptr) {
  for (int i = 0; i < num_arenas; i++) {
    if ((uintptr_t)ptr >= (uintptr_t)arenas[i].allocator.heap &&
        (uintptr_t)ptr < (uintptr_t)(arenas[i].allocator.heap +
                                     arenas[i].allocator.heap_size)) {
      return &arenas[i];
    }
  }
  return NULL;
}

static void deallocate_memory(Arena *arena, void *ptr) {
  BitmapAllocator *allocator = &arena->allocator;
  void *actual_start = (char *)ptr - sizeof(size_t);
  size_t block_index = ((uint8_t *)actual_start - allocator->heap) / BLOCK_SIZE;
  size_t size = *(size_t *)actual_start;
  size_t blocks_to_free = (size + sizeof(size_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;

  if (block_index + blocks_to_free <= allocator->heap_size / BLOCK_SIZE) {
    free_blocks(allocator, block_index, blocks_to_free);
  }
}

/* tinyfree */
void tinyfree(void *ptr) {
  if (ptr == NULL)
    return;

  pthread_mutex_lock(&malloc_mutex);

  // find the arena that this pointer belongs to
  Arena *arena = find_arena_for_pointer(ptr);

  if (arena == NULL) {
    pthread_mutex_unlock(&malloc_mutex);
    return; // invalid pointer
  }

  if (arena != NULL) {
    deallocate_memory(arena, ptr);
  }

  pthread_mutex_unlock(&malloc_mutex);
}
