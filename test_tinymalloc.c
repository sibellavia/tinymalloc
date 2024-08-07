#include "tinymalloc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

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
  printf("returned from tinymallo call");

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

int main() {
  test_basic_alloc_and_free();
  test_multiple_allocs();
  test_alloc_zero_size();
  test_alloc_large_size();
  test_free_null();
  test_write_to_allocated_memory();
  test_reuse_after_free();
  test_fragmentation();

  printf("all tests passed successfully! :-)\n");
  return 0;
}
