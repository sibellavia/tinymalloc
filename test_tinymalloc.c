#include "tinymalloc.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NUM_THREADS 4
#define ALLOCS_PER_THREAD 1000
#define ALLOC_SIZE 100
#define MOCK_HEAP_SIZE 1048576

typedef struct {
  int thread_id;
  void **ptrs;
  int num_allocs;
} ThreadData;

void test_basic_alloc_and_free() {
  printf("testing basic allocation and free...\n");
  void *ptr = tinymalloc(100);
  assert(ptr != NULL);
  tinyfree(ptr);
  printf("PASSED :-)\n\n");
}

void test_multiple_allocs() {
  printf("testing multiple allocations...\n");
  void *ptr1 = tinymalloc(100);
  void *ptr2 = tinymalloc(200);
  void *ptr3 = tinymalloc(300);
  assert(ptr1 != NULL);
  assert(ptr2 != NULL);
  assert(ptr3 != NULL);
  assert(ptr1 != ptr2 && ptr2 != ptr3 && ptr1 != ptr3);
  tinyfree(ptr1);
  tinyfree(ptr2);
  tinyfree(ptr3);
  printf("PASSED :-)\n\n");
}

void test_alloc_zero_size() {
  printf("testing allocation of zero size...\n");
  void *ptr = tinymalloc(0);
  assert(ptr == NULL);
  printf("PASSED :-)\n\n");
}

void test_alloc_large_size() {
  printf("testing allocation of large size...\n");
  printf("about to call tinymalloc for large allocation\n");
  void *ptr = tinymalloc(1024 * 1024); // 1mb
  printf("returned from tinymalloc\n");

  if (ptr == NULL) {
    printf("large allocation failed\n");
    printf("FAILED: unable to allocate large block :-(\n\n");
    return;
  } else {
    printf("large allocation succeded\n");
    assert(ptr != NULL);
    printf("about to free large allocation\n");
    tinyfree(ptr);
    printf("large allocation freed\n");
  }
  printf("PASSED :-)\n\n");
}

void test_free_null() {
  printf("testing free of NULL pointer...\n");
  tinyfree(NULL); // Should not crash
  printf("PASSED :-)\n\n");
}

void test_write_to_allocated_memory() {
  printf("testing writing to allocated memory...\n");
  char *ptr = (char *)tinymalloc(100);
  assert(ptr != NULL);
  strcpy(ptr, "Hello, World!");
  assert(strcmp(ptr, "Hello, World!") == 0);
  tinyfree(ptr);
  printf("PASSED :-)\n\n");
}

void test_reuse_after_free() {
  printf("testing memory reuse after free...\n");
  void *ptr1 = tinymalloc(100);
  tinyfree(ptr1);
  void *ptr2 = tinymalloc(100);
  assert(ptr1 == ptr2);
  tinyfree(ptr2);
  printf("PASSED :-)\n\n");
}

void test_fragmentation() {
  printf("testing fragmentation handling...\n");
  void *ptr1 = tinymalloc(100);
  void *ptr2 = tinymalloc(200);
  void *ptr3 = tinymalloc(300);
  tinyfree(ptr2);
  void *ptr4 = tinymalloc(150);
  assert(ptr4 != NULL);
  tinyfree(ptr1);
  tinyfree(ptr3);
  tinyfree(ptr4);
  printf("PASSED :-)\n\n");
}

void test_different_sizes() {
  printf("testing allocations of different sizes...\n");
  void *ptr1 = tinymalloc(10);
  void *ptr2 = tinymalloc(100);
  void *ptr3 = tinymalloc(1000);
  void *ptr4 = tinymalloc(10000);
  assert(ptr1 != NULL && ptr2 != NULL && ptr3 != NULL && ptr4 != NULL);
  assert(ptr1 != ptr2 && ptr2 != ptr3 && ptr3 != ptr4);
  tinyfree(ptr1);
  tinyfree(ptr2);
  tinyfree(ptr3);
  tinyfree(ptr4);
  printf("PASSED :-)\n\n");
}

void test_alignment() {
  printf("testing memory alignment...\n");
  void *ptr = tinymalloc(100);
  assert(ptr != NULL);
  assert(((uintptr_t)ptr % sizeof(void *)) == 0);
  tinyfree(ptr);
  printf("PASSED :-)\n\n");
}

void *thread_alloc_free(void *arg) {
  (void)arg;
  for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
    void *ptr = tinymalloc(ALLOC_SIZE);
    assert(ptr != NULL);
    tinyfree(ptr);
  }
  return NULL;
}

void test_multithreaded() {
  printf("testing multithreaded allocations...\n");
  pthread_t threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, thread_alloc_free, NULL);
  }
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }
  printf("PASSED :-)\n\n");
}

void test_boundary_conditions() {
  printf("Testing boundary conditions...\n");
  void *ptr1 = tinymalloc(1);
  assert(ptr1 != NULL);

  // Test a large, but not maximum, allocation
  size_t large_size = 1024 * 1024 * 1024; // 1 GB
  void *ptr2 = tinymalloc(large_size);
  if (ptr2 == NULL) {
    printf("Note: Large allocation (1GB) failed. This may be expected "
           "depending on system resources.\n");
  } else {
    tinyfree(ptr2);
  }

  tinyfree(ptr1);
  printf("PASSED :-)\n\n");
}

void *allocation_thread(void *arg) {
  ThreadData *data = (ThreadData *)arg;
  for (int i = 0; i < data->num_allocs; i++) {
    data->ptrs[i] = tinymalloc(1000); // Allocate larger chunks
    assert(data->ptrs[i] != NULL);
    printf("Thread %d, Allocation %d: pointer %p\n", data->thread_id, i,
           data->ptrs[i]);
    usleep(1000); // Small delay to increase chance of thread switching
  }
  return NULL;
}

void test_multi_arena_distribution() {
  printf("Testing multi-arena distribution...\n");

  pthread_t threads[NUM_THREADS];
  ThreadData thread_data[NUM_THREADS];
  void *all_ptrs[NUM_THREADS * ALLOCS_PER_THREAD];

  for (int i = 0; i < NUM_THREADS; i++) {
    thread_data[i].thread_id = i;
    thread_data[i].ptrs = &all_ptrs[i * ALLOCS_PER_THREAD];
    thread_data[i].num_allocs = ALLOCS_PER_THREAD;
    pthread_create(&threads[i], NULL, allocation_thread, &thread_data[i]);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  int unique_regions = 0;
  uintptr_t last_region_start = 0;
  uintptr_t region_starts[32] = {0}; // Assuming max 32 regions

  for (int i = 0; i < NUM_THREADS * ALLOCS_PER_THREAD; i++) {
    uintptr_t region_start = (uintptr_t)all_ptrs[i] & ~(MOCK_HEAP_SIZE - 1);
    if (region_start != last_region_start) {
      int found = 0;
      for (int j = 0; j < unique_regions; j++) {
        if (region_starts[j] == region_start) {
          found = 1;
          break;
        }
      }
      if (!found) {
        region_starts[unique_regions++] = region_start;
        last_region_start = region_start;
        printf("New unique region found: 0x%lx\n", region_start);
      }
    }
  }

  printf("Number of unique regions: %d\n", unique_regions);
  printf("Region starts: ");
  for (int i = 0; i < unique_regions; i++) {
    printf("0x%lx ", region_starts[i]);
  }
  printf("\n");

  // Check if allocations are distributed across multiple arenas
  assert(unique_regions > 1); // Ensure more than one arena was used

  for (int i = 0; i < NUM_THREADS * ALLOCS_PER_THREAD; i++) {
    tinyfree(all_ptrs[i]);
  }

  printf("PASSED :-)\n\n");
}

void test_find_suitable_arena() {
  printf("Testing arena selection for different sizes...\n");

  // Allocate a large block
  void *large_ptr = tinymalloc(MOCK_HEAP_SIZE / 2);
  assert(large_ptr != NULL);

  // Now allocate a small block, it should go to a different arena
  void *small_ptr = tinymalloc(100);
  assert(small_ptr != NULL);

  // Check that small_ptr is not in the same arena as large_ptr
  uintptr_t large_arena = (uintptr_t)large_ptr & ~(MOCK_HEAP_SIZE - 1);
  uintptr_t small_arena = (uintptr_t)small_ptr & ~(MOCK_HEAP_SIZE - 1);

  assert(large_arena != small_arena);

  tinyfree(large_ptr);
  tinyfree(small_ptr);

  printf("PASSED :-)\n\n");
}

void *stress_thread(void *arg) {
  int id = *(int *)arg;
  for (int i = 0; i < 10000; i++) {
    void *ptr = tinymalloc((id * 100) % 1000 +
                           1); // Different sizes for different threads
    assert(ptr != NULL);
    tinyfree(ptr);
  }
  return NULL;
}

void test_load_balancing_stress() {
  printf("Testing load balancing under stress...\n");

  int num_threads = 16; // Adjust based on your system
  pthread_t threads[num_threads];
  int thread_ids[num_threads];

  for (int i = 0; i < num_threads; i++) {
    thread_ids[i] = i;
    pthread_create(&threads[i], NULL, stress_thread, &thread_ids[i]);
  }

  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  // We can't directly check arena usage, but we can verify that the test
  // completes without errors
  printf("PASSED :-)\n\n");
}

int main() {
  test_basic_alloc_and_free();
  test_multiple_allocs();
  test_alloc_zero_size();
  test_alloc_large_size();
  test_free_null();
  test_write_to_allocated_memory();
  test_reuse_after_free();
  test_fragmentation();
  test_different_sizes();
  test_alignment();
  test_multithreaded();
  test_boundary_conditions();
  test_multi_arena_distribution();
  test_find_suitable_arena();
  test_load_balancing_stress();

  printf("all tests passed successfully! :-)\n");
  return 0;
}
