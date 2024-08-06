#include "tinymalloc.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define INITIAL_HEAP_SIZE (1024 * 1024)

/* Mutex */
atomic_int mutex_locked = 0;
pthread_mutex_t tinymalloc_mutex = PTHREAD_MUTEX_INITIALIZER;

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
  int max_iterations = 1000000;
  int i = 0;

  while (current != NULL) {
    if (current->size >= size && current->is_free) {
      if (prev == NULL) {
        free_list = current->next_free;
      } else {
        prev->next_free = current->next_free;
      }
      return current;
    }

    prev = current;
    current = current->next_free;
    if (++i >= max_iterations) {
      printf("ERROR - possible infinite loop detected in find_free_block\n");
      return NULL;
    }
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
  printf("DEBUG - entering remove_from_free_list\n");
  printf("DEBUG - block to remove: %p\n", (void *)block);
  printf("DEBUG - current free_list: %p\n", (void *)free_list);

  if (free_list == NULL) {
    printf("ERROR - free_list is NULL\n");
    return;
  }

  if (free_list == block) {
    // block to remove is at the head of the free list
    printf("DEBUG - removing from head of free list\n");
    free_list = block->next_free;
  } else {
    struct block_header *current = free_list;
    struct block_header *prev = NULL;
    int max_iterations = 1000000;
    int i = 0;
    while (current != NULL && current != block) {
      printf("DEBUG - traversing free list, current: %p\n", (void *)current);
      prev = current;
      current = current->next_free;
      if (++i >= max_iterations) {
        printf(
            "ERROR - possible infinite loop detected in remove_from_free_list");
        return;
      }
    }

    if (current == block) {
      printf("DEBUG - found block in free list, removing\n");
      current->next_free = block->next_free;
    } else {
      printf("DEBUG - block not found in free list\n");
      return;
    }
  }

  block->next_free = NULL;

  printf("DEBUG - exiting remove_from_free_list\n");
}

void coalesce(struct block_header *block) {
  printf("DEBUG - entering coalesce\n");
  struct block_header *next = block->next;

  printf("DEBUG - checking next block\n");
  if (next == NULL) {
    printf("DEBUG - no next block\n");
    return;
  }

  // coalesce with next block if free
  if (next->is_free) {
    printf("DEBUG - next block is free, coalescing\n");
    remove_from_free_list(next);

    // we combine the sizes
    block->size += next->size + sizeof(struct block_header);
    block->next = next->next;

    if (next == heap_end) {
      heap_end = block;
    }

    if (block->next_free == next) {
      block->next_free = next->next_free;
    }
  } else {
    printf("DEBUG - next block is not free or doesn't exist\n");
  }

  printf("DEBUG - exiting coalesce\n");
  // note that i'm not coalisce with the previous block becaue i didn't
  // implement a prev ptr so for a more complete implementation you might want
  // to add a prev ptr btw i hope to implement it in the future :-)
}

void validate_free_list() {
  struct block_header *slow = free_list;
  struct block_header *fast = free_list;
  int max_iterations = 1000000;
  int i = 0;

  while (fast != NULL && fast->next_free != NULL) {
    slow = slow->next_free;
    fast = fast->next_free->next_free;
    if (slow == fast) {
      printf("ERROR - circular reference detected in free list\n");
      return;
    }
    if (++i >= max_iterations) {
      printf("ERROR - free list too long, possible issue\n");
      return;
    }
  }
}

void tinyfree(void *ptr) {
  printf("DEBUG - entering tinyfree\n");

  printf("DEBUG - about to lock mutex in tinyfree\n");
  printf("DEBUG - mutex state before locking: %d\n",
         atomic_load(&mutex_locked));

  int attempts = 0;
  int max_attempts = 100; // Adjust this value as needed
  int lock_result;

  do {
    lock_result = pthread_mutex_trylock(&tinymalloc_mutex);
    if (lock_result == 0) {
      printf("DEBUG - mutex locked in tinyfree\n");
      atomic_store(&mutex_locked, 1);
      break;
    } else if (lock_result != EBUSY) {
      printf("DEBUG - unexpected error when trying to lock mutex in tinyfree: "
             "%s\n",
             strerror(lock_result));
      return; // Exit the function if we encounter an unexpected error
    }

    attempts++;
    if (attempts >= max_attempts) {
      printf("DEBUG - failed to lock mutex in tinyfree after %d attempts\n",
             max_attempts);
      return; // Exit the function if we can't lock the mutex after max attempts
    }

    // Sleep for a short time before trying again
    struct timespec ts = {0, 1000000}; // 1 millisecond
    nanosleep(&ts, NULL);
  } while (lock_result != 0);

  if (ptr == NULL) {
    printf("DEBUG - ptr is NULL, unlocking mutex and returning\n");
    pthread_mutex_unlock(&tinymalloc_mutex);
    atomic_store(&mutex_locked, 0);
    return;
  }

  printf("DEBUG - getting block header\n");
  struct block_header *block = (struct block_header *)ptr - 1;
  printf("DEBUG - block header retrieved\n");

  if (!block->is_free) {
    printf("DEBUG - setting block as free\n");
    block->is_free = 1;

    printf("DEBUG - adding block to free list\n");
    block->next_free = free_list;
    free_list = block;

    printf("DEBUG - calling coalesce\n");
    // we coalesce with neighboring block
    coalesce(block);
  } else {
    printf("ERROR - trying to free an already free block\n");
  }

  validate_free_list();

  printf("DEBUG - unblocking mutex in tinyfree\n");
  pthread_mutex_unlock(&tinymalloc_mutex);
  atomic_store(&mutex_locked, 0);
  printf("DEBUG - mutex unlocked in tinyfree\n");
  printf("DEBUG - exiting tinyfree");
}

/* tinymalloc */

void *tinymalloc(size_t size) {
  void *result;

  printf("DEBUG - entering tinymalloc\n");
  printf("DEBUG - about to lock mutex in tinymalloc\n");
  pthread_mutex_lock(&tinymalloc_mutex);
  atomic_store(&mutex_locked, 1);
  printf("DEBUG - mutex locked in tinymalloc\n");

  if (size == 0) {
    pthread_mutex_unlock(&tinymalloc_mutex);
    atomic_store(&mutex_locked, 0);
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

  if (block != NULL) {
    printf("DEBUG - suitable free block found at %p\n", (void *)block);
    printf("DEBUG - about to remove block from free list\n");
    remove_from_free_list(block);
    printf("DEBUG - block removed from free list\n");
  } else {
    // we have to extend the heap because we didn't find free blocks
    printf("DEBUG - no suitable free block found, extending heap\n");
    block = extend_heap(size);
    if (block == NULL) {
      pthread_mutex_unlock(&tinymalloc_mutex);
      atomic_store(&mutex_locked, 0);
      return NULL;
    }
  }

  // if the block is too big, we will split it
  if (block->size > size + sizeof(struct block_header) + sizeof(size_t)) {
    split_block(block, size);
  }

  block->is_free = 0;

  printf("DEBUG - about to remove block from free list\n");
  remove_from_free_list(block);
  printf("DEBUG - block removed from free list\n");

  printf("DEBUG - about to return from tinymalloc\n");

  validate_free_list();

  result = (void *)(block + 1);
  printf("DEBUG - about to unlock mutex in tinymalloc\n");
  pthread_mutex_unlock(&tinymalloc_mutex);
  atomic_store(&mutex_locked, 0);
  printf("DEBUG - mutex unlocked in tinymalloc\n");
  printf("DEBUG - exiting tinymalloc\n");
  return result;
}
