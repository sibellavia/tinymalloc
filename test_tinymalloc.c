#include "tinymalloc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_tinymalloc_basic() {
  printf("running basic tinymalloc tests...\n");

  // test 1: allocate a small block
  int *p1 = (int *)tinymalloc(sizeof(int));
  assert(p1 != NULL);
  *p1 = 42;
  assert(*p1 == 42);
  printf("test 1 passed: allocated small block and wrote to it\n");

  // test 2: allocate a larger block
  int *arr = (int *)tinymalloc(10 * sizeof(int));
  assert(arr != NULL);
  for (int i = 0; i < 10; i++) {
    arr[i] = i;
  }
  for (int i = 0; i < 10; i++) {
    assert(arr[i] == i);
  }
  printf("test 2 passed: allocated larger block and wrote to it\n");

  // test 3: allocate zero bytes (should return NULL)
  void *p2 = tinymalloc(0);
  assert(p2 == NULL);
  printf("test 3 passed: allocating 0 bytes returns NULL\n");

  // test 4: allocate a very large block
  void *p3 = tinymalloc(1000000);
  assert(p3 != NULL);
  printf("test 4 passed: allocated very large block\n");

  // Free all allocated memory
  tinyfree(p1);
  tinyfree(arr);
  tinyfree(p3);
  printf("basic tinymalloc tests completed successfully :-)\n\n");
}

void test_tinyfree() {
  printf("running tinyfree tests...\n");

  // allocate and free a single block
  int *p1 = (int *)tinymalloc(sizeof(int));
  assert(p1 != NULL);
  tinyfree(p1);
  printf("test 1 passed: allocated and freed a single block\n");

  // allocate multiple blocks and free them
  int *p2 = (int *)tinymalloc(sizeof(int));
  int *p3 = (int *)tinymalloc(sizeof(int));
  int *p4 = (int *)tinymalloc(sizeof(int));
  assert(p2 != NULL && p3 != NULL && p4 != NULL);
  tinyfree(p2);
  tinyfree(p3);
  tinyfree(p4);
  printf("test 2 passed: allocated and freed multiple blocks\n");

  // test freeing NULL
  tinyfree(NULL);
  printf("test 3 passed: Freed NULL pointer\n");

  printf("tinyfree tests completed successfully :-)\n\n");
}

void test_coalescing() {
  printf("Running coalescing tests...\n");

  // Allocate 3 blocks
  int *p1 = (int *)tinymalloc(sizeof(int));
  int *p2 = (int *)tinymalloc(sizeof(int));
  int *p3 = (int *)tinymalloc(sizeof(int));
  assert(p1 != NULL && p2 != NULL && p3 != NULL);

  // Free the middle block
  tinyfree(p2);

  // Free the first block, should coalesce with the second
  tinyfree(p1);

  // Allocate a larger block, should fit in the coalesced space
  int *p4 = (int *)tinymalloc(2 * sizeof(int));
  assert(p4 != NULL);
  assert(p4 == p1 || p4 == p2); // The new block should start at p1 or p2

  printf("Coalescing test passed\n\n");

  // Clean up
  tinyfree(p3);
  tinyfree(p4);
}

int main() {
  test_tinymalloc_basic();
  test_tinyfree();
  test_coalescing();
  printf("All tests completed successfully!\n");
  return 0;
}
