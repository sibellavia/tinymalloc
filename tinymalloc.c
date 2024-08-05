#include "tinymalloc.h"
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#define INITIAL_HEAP_SIZE (1024 * 1024)

/* Block Structure */

struct block_header {
  size_t size;
  struct block_header *next;
  struct block_header *next_free;
  int is_free;
};

struct block_header *heap_start = NULL;
struct block_header *free_list = NULL;
struct block_header *heap_end = NULL;

pthread_mutex_t tinymalloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/* initialize_heap() */
void *initialize_heap() {
  void *mem = sbrk(INITIAL_HEAP_SIZE);
  if (mem == (void *)-1)
    return NULL;

  struct block_header *heap = (struct block_header *)mem;
  heap->size = INITIAL_HEAP_SIZE - sizeof(struct block_header);
  heap->is_free = 1;
  heap->next = NULL;
  heap->next_free = NULL;

  heap_start = heap;
  heap_end = heap;

  return heap;
}

/* find_free_block() */
struct block_header *find_free_block(size_t size) {
  struct block_header *current = free_list;
  struct block_header *prev = NULL;

  while (current != NULL) {
    if (current->size >= size) {
      if (prev == NULL) {
        free_list = current->next_free;
      } else {
        prev->next_free = current->next_free;
      }
      return current;
    }

    prev = current;
    current = current->next_free;
  }

  return NULL; // no suitable block found
}

/* extend_heap() */

struct block_header *extend_heap(size_t size) {
  size_t total_size = size + sizeof(struct block_header);
  void *mem = sbrk(total_size);

  if (mem == (void *)-1) {
    return NULL;
  }

  struct block_header *new_block = (struct block_header *)mem;
  new_block->size = size;
  new_block->is_free = 1;
  new_block->next = NULL;
  new_block->next_free = NULL;

  if (heap_start == NULL) {
    heap_start = new_block;
    heap_end = new_block;
  } else {
    heap_end->next = new_block;
    heap_end = new_block;
  }

  // add to free list
  new_block->next_free = free_list;
  free_list = new_block;

  return new_block;
}

/* split_block() */

struct block_header *split_block(struct block_header *block, size_t size) {
  size_t remaining_size = block->size - (sizeof(struct block_header) + size);

  if (remaining_size < sizeof(struct block_header) + sizeof(size_t)) {
    return NULL; // not enough space to split
  }

  struct block_header *new_block =
      (struct block_header *)((char *)block + sizeof(struct block_header) +
                              size);
  new_block->size = remaining_size - sizeof(struct block_header);
  new_block->is_free = 1;
  new_block->next = block->next;
  new_block->next_free = NULL;

  if (block == heap_end) {
    heap_end = new_block;
  }

  block->next = new_block;
  block->size = size;

  new_block->next_free = free_list;
  free_list = new_block;

  return block;
}

void remove_from_free_list(struct block_header *block) {
  if (free_list == block) {
    // block to remove is at the head of the free list
    free_list = block->next_free;
  } else {
    struct block_header *current = free_list;
    while (current->next_free != NULL && current->next_free != block) {
      current = current->next_free;
    }

    if (current->next_free == block) {
      current->next_free = block->next_free;
    }
  }

  block->next_free = NULL;
}

/* tinymalloc */

void *tinymalloc(size_t size) {
  void *result;
  pthread_mutex_lock(&tinymalloc_mutex);

  if (size == 0) {
    return NULL;
  }

  // align the size
  size = (size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

  if (heap_start == NULL) {
    heap_start = initialize_heap();
    if (heap_start == NULL)
      return NULL;
    heap_end = heap_start;
    free_list = heap_start;
  }

  struct block_header *block = find_free_block(size);

  if (block == NULL) {
    // we have to extend the heap because we didn't find free blocks
    block = extend_heap(size);
    if (block == NULL) {
      return NULL;
    }
    // i could update heap_end, but is already updated in extend_heap
  }

  // if the block is too big, we will split it
  if (block->size > size + sizeof(struct block_header) + sizeof(size_t)) {
    split_block(block, size);
  }

  block->is_free = 0;

  remove_from_free_list(block);

  return (void *)(block + 1);
  pthread_mutex_unlock(&tinymalloc_mutex);
  return result;
}

void coalesce(struct block_header *block) {
  struct block_header *next = block->next;

  // coalesce with next block if free
  if (next && next->is_free) {
    remove_from_free_list(next);

    // we combine the sizes
    block->size += next->size + sizeof(struct block_header);
    block->next = next->next;

    if (next == heap_end) {
      heap_end = block;
    }
  }

  // note that i'm not coalisce with the previous block becaue i didn't
  // implement a prev ptr so for a more complete implementation you might want
  // to add a prev ptr btw i hope to implement it in the future :-)
}

void tinyfree(void *ptr) {
  pthread_mutex_lock(&tinymalloc_mutex);
  if (ptr == NULL) {
    return;
  }

  // we get the block header
  struct block_header *block = (struct block_header *)ptr - 1;

  block->is_free = 1;

  block->next_free = free_list;
  free_list = block;

  // we coalesce with neighboring block
  coalesce(block);
  pthread_mutex_unlock(&tinymalloc_mutex);
}
