#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "tinymalloc.h"

#define TEST_ALLOCS 100000
#define MAX_ALLOC_SIZE 1024

double run_test(int use_tinymalloc) {
    void* ptrs[TEST_ALLOCS];
    clock_t start, end;
    double cpu_time_used;

    start = clock();

    for (int i = 0; i < TEST_ALLOCS; i++) {
        size_t size = rand() % MAX_ALLOC_SIZE + 1;
        if (use_tinymalloc) {
            printf("Allocating %zu bytes with tinymalloc, iteration %d\n", size, i);
            ptrs[i] = tinymalloc(size);
            printf("DEBUG - allocation completed for iteration %d\n", i);
        } else {
            ptrs[i] = malloc(size);
        }

        if (ptrs[i] == NULL) {
            printf("Allocation failed at iteration %d\n", i);
            exit(1);
        }

        // Write some data to ensure the memory is accessible
        memset(ptrs[i], 0, size);

        // Free some allocations to create fragmentation
        if (i % 2 == 0) {
            if (use_tinymalloc) {
                printf("Freeing allocation %d\n", i);
                tinyfree(ptrs[i]);
            } else {
                free(ptrs[i]);
            }
            ptrs[i] = NULL;
        }

        if (use_tinymalloc && i % 1000 == 0) {
            printf("Completed %d iterations\n", i);
        }
    }

    // Free remaining allocations
    for (int i = 0; i < TEST_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            if (use_tinymalloc) {
                printf("Final free of allocation %d\n", i);
                tinyfree(ptrs[i]);
            } else {
                free(ptrs[i]);
            }
        }
    }

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    return cpu_time_used;
}

int main() {
    srand(time(NULL));

    printf("Running tests...\n");

    double time_malloc = run_test(0);
    printf("Standard malloc took %f seconds\n", time_malloc);

    printf("Starting tinymalloc test...\n");
    double time_tinymalloc = run_test(1);
    printf("Tinymalloc took %f seconds\n", time_tinymalloc);

    printf("Tinymalloc is %.2f%% %s than standard malloc\n", 
           fabs(time_tinymalloc - time_malloc) / time_malloc * 100,
           time_tinymalloc < time_malloc ? "faster" : "slower");

    return 0;
}
