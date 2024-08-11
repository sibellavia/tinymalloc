#ifndef TINYMALLOC_H
#define TINYMALLOC_H

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

extern pthread_mutex_t malloc_mutex;

// public function prototypes
void *tinymalloc(size_t size);
void tinyfree(void *ptr);

#endif
