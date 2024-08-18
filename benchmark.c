#include "tinymalloc.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_ALLOCATIONS 1000000
#define NUM_THREADS 4

// Sizes to test (in bytes)
#define NUM_SIZES 5
const size_t TEST_SIZES[NUM_SIZES] = {16, 64, 256, 1024, 4096};

uint64_t get_time_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void benchmark_allocation_sizes(void *(*alloc_func)(size_t),
                                void (*free_func)(void *), const char *name) {
  for (int s = 0; s < NUM_SIZES; s++) {
    size_t size = TEST_SIZES[s];
    uint64_t start_time = get_time_ns();

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
      void *ptr = alloc_func(size);
      if (ptr == NULL) {
        printf("Allocation failed\n");
        return;
      }
      free_func(ptr);
    }

    uint64_t end_time = get_time_ns();
    printf("%s allocation/deallocation time for size %zu bytes: %lu ns\n", name,
           size, end_time - start_time);
  }
}

void *thread_func_sizes(void *arg) {
  size_t size = *(size_t *)arg;
  int num_allocs = NUM_ALLOCATIONS / NUM_THREADS;
  for (int i = 0; i < num_allocs; i++) {
    void *ptr = tinymalloc(size);
    if (ptr == NULL) {
      printf("Allocation failed in thread\n");
      return NULL;
    }
    tinyfree(ptr);
  }
  return NULL;
}

void benchmark_multithreaded_sizes() {
  for (int s = 0; s < NUM_SIZES; s++) {
    size_t size = TEST_SIZES[s];
    pthread_t threads[NUM_THREADS];
    uint64_t start_time = get_time_ns();

    for (int i = 0; i < NUM_THREADS; i++) {
      if (pthread_create(&threads[i], NULL, thread_func_sizes, &size) != 0) {
        printf("Failed to create thread\n");
        return;
      }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
      pthread_join(threads[i], NULL);
    }

    uint64_t end_time = get_time_ns();
    printf("Multi-threaded tinymalloc allocation/deallocation time for size "
           "%zu bytes: %lu ns\n",
           size, end_time - start_time);
  }
}

int main() {
  printf("Starting memory allocator benchmarks...\n\n");

  printf("Single-threaded tests:\n");
  printf("----------------------\n");
  benchmark_allocation_sizes(tinymalloc, tinyfree, "tinymalloc");
  benchmark_allocation_sizes(malloc, free, "system malloc");

  printf("\nMulti-threaded tests (tinymalloc only):\n");
  printf("----------------------------------------\n");
  benchmark_multithreaded_sizes();

  printf("\nBenchmarks complete.\n");
  return 0;
}
