#include "tinymalloc.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define HEAP_SIZE (1024 * 1024)

/* Mutex */
pthread_mutex_t tinymalloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Block Structure */
struct memory_block {
  size_t size;
  struct memory_block *next;
  struct memory_block *prev;
  int is_free;
};

struct memory_block *heap_head = NULL;
pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/* tinymalloc */

void *tinymalloc(size_t size) {
  pthread_mutex_lock(&malloc_mutex);

  printf("DEBUG - entered tinymalloc.\n");

  // align the size
  // we ensure that the requested size is algined to a multiple of
  // sizeof(size_t)
  size = (size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

  printf("DEBUG - size aligned: %zu\n", size);

  // v0.1 returns NULL, but in the future it should return a
  // non-NULL pointer that can be valid for tinyfree
  if (size == 0) {
    pthread_mutex_unlock(&malloc_mutex);
    return NULL;
  }

  if (heap_head == NULL) {
    printf("DEBUG - heap non-existent. we start the initialization.\n");

    // we initialize the heap
    heap_head = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heap_head == MAP_FAILED) {
      pthread_mutex_unlock(&malloc_mutex);
      return NULL;
    }

    heap_head->size = HEAP_SIZE - sizeof(struct memory_block);
    heap_head->next = NULL;
    heap_head->prev = NULL;
    heap_head->is_free = 1;

    printf("DEBUG - heap now exists.\n");
  }

  // first-fit search
  struct memory_block *current = heap_head;

  printf("DEBUG - we start the first-fit search.\n");

  struct memory_block *last_block = current->prev;

  while (current) {
    last_block = current;
    if (current->is_free && current->size >= size) {
      printf("DEBUG - block found. we try to split it, if possible.\n");
      // split if possible
      if (current->size >=
          size + sizeof(struct memory_block) + sizeof(size_t)) {

        printf("DEBUG - split is possible, so we proceed.\n");

        struct memory_block *new_block =
            (struct memory_block *)((char *)current +
                                    sizeof(struct memory_block) + size);
        new_block->size = current->size - size - sizeof(struct memory_block);
        new_block->is_free = 1;
        new_block->next = current->next;
        new_block->prev = current;

        if (current->next) {
          current->next->prev = new_block;
        }
        current->size = size;
        current->next = new_block;
      }

      printf("DEBUG - split is not possible. we take current block as-is.\n");

      current->is_free = 0;
      current->size = size;

      printf("DEBUG - we prepare to unlock mutex\n");
      pthread_mutex_unlock(&malloc_mutex);
      printf("DEBUG - mutex unlocked. we return.\n");
      return (void *)(current + 1);
    }
    current = current->next;
  }

  if (last_block != NULL) {
    size_t block_size = size + sizeof(struct memory_block);
    struct memory_block *more_memory = NULL;
    more_memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (more_memory == MAP_FAILED) {
      pthread_mutex_unlock(&malloc_mutex);
      return NULL; // OOM
    }

    more_memory->size = size;
    more_memory->is_free = 0;
    more_memory->next = NULL;
    more_memory->prev = last_block;

    last_block->next = more_memory;
    printf("DEBUG - we prepare to unlock mutex\n");
    pthread_mutex_unlock(&malloc_mutex);
    printf("DEBUG - mutex unlocked. we return newly allocated memory.\n");
    return (void *)(more_memory + 1);
  }

  printf("DEBUG - we prepare to unlock mutex\n");
  pthread_mutex_unlock(&malloc_mutex);
  printf("DEBUG - mutex unlocked. we exit. out-of-memory or unexpected error "
         "occurred.\n");
  return NULL; // OOM
}

void tinyfree(void *ptr) {
  if (!ptr)
    return;

  printf("DEBUG - entered tinyfree. preparing to lock mutex\n");
  pthread_mutex_lock(&malloc_mutex);
  printf("DEBUG - mutex locked, we continue \n");
  struct memory_block *block = (struct memory_block *)ptr - 1;
  block->is_free = 1;

  // coalesce with next block
  if (block->next && block->next->is_free) {
    printf("DEBUG - we coalesce with next block\n");
    block->size += sizeof(struct memory_block) + block->next->size;
    block->next = block->next->next;
    if (block->next) {
      block->next->prev = block;
    }
  }

  // coalesce with previous block-
  if (block->prev && block->prev->is_free) {
    printf("DEBUG - we coalesce with previous block\n");
    block->size += sizeof(struct memory_block) + block->prev->size;
    block->prev->next = block->next;
    if (block->next) {
      block->next->prev = block->prev;
    }
  }

  printf("DEBUG - we unlock mutex\n");
  pthread_mutex_unlock(&malloc_mutex);
  printf("DEBUG - we unlocked mutex and we exit tinyfree\n");
}
