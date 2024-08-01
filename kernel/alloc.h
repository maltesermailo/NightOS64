//
// Created by Jannik on 10.03.2024.
//

#ifndef NIGHTOS_ALLOC_H
#define NIGHTOS_ALLOC_H

#include <stddef.h>
#include "alloc/liballoc.h"

/**
 * Kernel modules should use this alloc function to allocate known sizes
 * @param size the known size
 * @return a memory region
 */
void* kmalloc(size_t size);
/**
 * This returns a known memory size region to a slab
 * @param ptr the ptr
 */
void kfree(void* ptr);

/**
 * Registers a new object size in the allocator.
 * The allocator will make sure to have at least 1 to n pre-allocated in a pool, where n is the number of division possible in one page
 * The allocator uses a 64-bit integer to track state of each element after the header. So at most n can be 64.
 * @param size the size of the individual object
 */
void alloc_register_object_size(size_t size);

#define SLAB_SIZE 4096

struct slab {
    void* memory;
    size_t object_size;
    uint8_t total_objects;
    uint8_t free_objects;
    unsigned long* bitmap;  // To track free/used objects
    struct slab* next;
};

struct size_class {
    size_t size;
    struct slab* slabs;
};

#define MEM_BLOCK_MAGIC 0x1BAB0

#endif //NIGHTOS_ALLOC_H
