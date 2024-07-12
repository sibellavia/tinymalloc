#include <stdio.h>
#include "tinymalloc.h"

int main() {
    int *arr1 = (int*)tinymalloc(5 * sizeof(int));
    if (arr1 == NULL) {
        printf("Failed to allocate arr1\n");
        return 1;
    }
    printf("Successfully allocated arr1\n");

    int *arr2 = (int*)tinymalloc(10 * sizeof(int));
    if (arr2 == NULL) {
        printf("Failed to allocate arr2\n");
        return 1;
    }
    printf("Successfully allocated arr2\n");

    // Use the allocated memory
    for (int i = 0; i < 5; i++) {
        arr1[i] = i;
    }
    for (int i = 0; i < 10; i++) {
        arr2[i] = i * 2;
    }

    // Print some values to verify
    printf("arr1[2] = %d\n", arr1[2]);
    printf("arr2[5] = %d\n", arr2[5]);

    return 0;
}
