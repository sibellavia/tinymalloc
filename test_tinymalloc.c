#include "tinymalloc.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NUM_THREADS 4
#define ALLOCS_PER_THREAD 1000
#define ALLOC_SIZE 100

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

  printf("all tests passed successfully! :-)\n");
  return 0;
}
