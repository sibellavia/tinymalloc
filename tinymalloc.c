#include "tinymalloc.h"
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#define INITIAL_HEAP_SIZE (1024 * 1024)
#define THRESHOLD (1024)
#define EXPANSION_SIZE (1024 * 1024)

/* Block Structure */

struct block_header {
  size_t size;
  int is_free;
  struct block_header *next;
  struct block_header *next_free;
};

struct block_header *heap_start = NULL;
struct block_header *free_list = NULL;

/* initialize_heap() */
void *initialize_heap() {
  void *mem = sbrk(INITIAL_HEAP_SIZE);
  if (mem == (void *)-1)
    return NULL;

  struct block_header *heap = (struct block_header *)mem;
  heap->size = INITIAL_HEAP_SIZE - sizeof(struct block_header);
  heap->is_free = 1;
  heap->next = NULL;

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
  } else {
    struct block_header *current = heap_start;
    while (current->next != NULL) {
      current = current->next;
    }
    current->next = new_block;
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

  new_block->next = block->next;
  block->next = new_block;
  block->size = size;

  new_block->next_free = free_list;
  free_list = new_block;

  return block;
}

/* tinymalloc */

void *tinymalloc(size_t size) {
  if (size == 0) {
    return NULL;
  }

  size = (size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

  if (heap_start == NULL) {
    heap_start = initialize_heap();
    if (heap_start == NULL)
      return NULL;
    free_list = heap_start;
  }

  struct block_header *block = find_free_block(size);

  if (block == NULL) {
    // we have to extend the heap because we didn't find free blocks
    block = extend_heap(size);
    if (block == NULL) {
      return NULL;
    }
  }

  // if the block is too big, we will split it
  if (block->size > size + sizeof(struct block_header) + sizeof(size_t)) {
    split_block(block, size);
  }

  block->is_free = 0;

  remove_from_free_list(block);

  return (void *)(block + 1);
}
