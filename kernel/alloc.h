//
// Created by Jannik on 10.03.2024.
//

#ifndef NIGHTOS_ALLOC_H
#define NIGHTOS_ALLOC_H

#define _HAVE_SIZE_T

#include <stddef.h>
#include "alloc/liballoc.h"

void* kmalloc(size_t size);
void kfree(void* ptr);

/*typedef struct MemoryAlloc {
    int magic;
    size_t len;
} memory_alloc_t;

typedef struct FreeMemoryBlock {
    size_t base;
    size_t len;
    struct FreeMemoryBlock* next;
} free_memory_block_t;*/

#define MEM_BLOCK_MAGIC 0x1BAB0

#endif //NIGHTOS_ALLOC_H
