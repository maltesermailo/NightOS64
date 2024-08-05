//
// Created by Jannik on 17.03.2024.
//
#include "../../memmgr.h"
#include "../../lock.h"

#define MAX_SIZE_CLASSES 512

static struct size_class registeredSizeClasses[MAX_SIZE_CLASSES];

struct slab* kmalloc_for_size(struct size_class* sizeClass) {
    uint8_t perPage = (4096 / sizeClass->size);
    uint8_t bitmapSize = perPage / 8;
    if(bitmapSize % 8 != 0) {
        bitmapSize = (bitmapSize + 8) & ~(8 - 1);
    }

    struct slab* slab = malloc(sizeof(struct slab));
    memset(slab, 0, sizeof(struct slab));

    slab->total_objects = perPage;
    slab->free_objects = perPage;
    slab->object_size = sizeClass->size;

    slab->memory = sbrk(4096);
    slab->bitmap = malloc(bitmapSize);

    struct slab* ptr = sizeClass->slabs;
    if(ptr == NULL) {
        sizeClass->slabs = slab;
    } else {
        while(ptr->next) {
            ptr = ptr->next;
        }

        ptr->next = slab;
    }

    return slab;
}

void* kmalloc(size_t size) {
    for(int i = 0; i < MAX_SIZE_CLASSES; i++) {
        if(registeredSizeClasses[i].size == size) {
            struct slab* slab = registeredSizeClasses[i].slabs;

            while(slab) {
                if(slab->free_objects > 0) {
                    for(int j = 0; j < slab->total_objects; j++) {
                        int index = (j / 64);
                        int bit = (j % 64);

                        unsigned long bitmap = slab->bitmap[index];

                        if((bitmap & (1 << bit)) == 0) {
                            slab->bitmap[index] |= (1 << bit);
                            slab->free_objects--;

                            return slab->memory + (64 * index * slab->object_size) + (bit * slab->object_size);
                        }
                    }
                } else {
                    slab = slab->next;
                }
            }

            //If we got here, no slabs are free
            slab = registeredSizeClasses[i].slabs;
            while(slab->next) {
                slab = slab->next;
            }

            slab->next = kmalloc_for_size(&registeredSizeClasses[i]);
            slab->bitmap[0] |= 1 << 0;
            slab->free_objects--;

            return slab->memory;
        }
    }

    return NULL;
}

void kfree(void* ptr) {
    if (ptr == NULL) return;

    for (int i = 0; i < MAX_SIZE_CLASSES; i++) {
        if (registeredSizeClasses[i].size > 0) {
            struct slab* slab = registeredSizeClasses[i].slabs;

            while (slab) {
                if (ptr >= slab->memory && ptr < slab->memory + 4096) {
                    // Found the correct slab
                    ptrdiff_t offset = (char*)ptr - (char*)slab->memory;
                    int objectIndex = offset / slab->object_size;
                    int bitmapIndex = objectIndex / 64;
                    int bitOffset = objectIndex % 64;

                    // Clear the bit in the bitmap
                    slab->bitmap[bitmapIndex] &= ~(1UL << bitOffset);

                    slab->free_objects++;

                    return;
                }
                slab = slab->next;
            }
        }
    }
}

void* kcalloc(int nobj, size_t size) {
    unsigned long long real_size;
    void *p;

    real_size = nobj * size;

    p = kmalloc( real_size );

    memset( p, 0, real_size );

    return p;
}

void alloc_register_object_size(size_t size) {
    for(int i = 0; i < MAX_SIZE_CLASSES; i++) {
        if(registeredSizeClasses[i].size == size) {
            return;
        }
    }

    for(int i = 0; i < MAX_SIZE_CLASSES; i++) {
        if(registeredSizeClasses[i].size == 0) {
            registeredSizeClasses[i].size = size;
            registeredSizeClasses[i].slabs = NULL;

            kmalloc_for_size(&registeredSizeClasses[i]);
            return;
        }
    }
}

const static int page_size = 0x1000;
const static spin_t LOCK = ATOMIC_FLAG_INIT;

int liballoc_lock()
{
    spin_lock(&LOCK);
    return 0;
}

int liballoc_unlock()
{
    spin_unlock(&LOCK);
    return 0;
}

void* liballoc_alloc( int pages )
{
    unsigned int size = pages * page_size;

    char *p2 = (char*) sbrk(size);
    if ( p2 == -1) return NULL;

    return p2;
}

int liballoc_free( void* ptr, int pages )
{
    munmap( ptr, pages * page_size );
    return 0;
}



