#ifndef TINYMALLOC_OLD_H
#define TINYMALLOC_OLD_H

#include <stddef.h>

void *tinymalloc_old(size_t size);
void tinyfree_old(void *ptr);

#endif // TINYMALLOC_OLD_H
