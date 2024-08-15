#ifndef TINYMALLOC_H
#define TINYMALLOC_H

#define _GNU_SOURCE
#include <sched.h>

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

extern pthread_mutex_t malloc_mutex;

/* Bitmap */
// uint8_t *heap: this is a pointer to the beginning of the memory heap
// uint64_t *bitmap: this is a pointer to the bitmap used to track which blocks
// in the heap are allocated or free
// size_t heap_size: stores the total size of the heap in bytes
// size_t bitmap_size: stores the size of the bitmap
// why i keep track of the sizes separately? bcos the bitmap size is not
// necessarily directly proportional to the heap size due to the use of 64-bit
// integers for efficiency
// many thanks to @basit_ayantunde for suggesting me the change from uint8_t to
// uint64_t for the bitmap
typedef struct {
  uint8_t *heap;
  uint64_t *bitmap;
  size_t heap_size;
  size_t bitmap_size;
  pthread_mutex_t mutex;
} BitmapAllocator;

extern BitmapAllocator allocator;

// public function prototypes
void *tinymalloc(size_t size);
void tinyfree(void *ptr);

#endif
