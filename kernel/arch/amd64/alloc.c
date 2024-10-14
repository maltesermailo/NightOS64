//
// Created by Jannik on 17.03.2024.
//
#include "../../memmgr.h"
#include "../../lock.h"
#include "../../../mlibc/abis/linux/errno.h"

#define MAX_SIZE_CLASSES 512
#define DEFAULT_SIZE_CLASSES 10

static struct size_class registeredSizeClasses[MAX_SIZE_CLASSES];
static struct size_class defaultSizeClasses[12] = {
        {.size = 8, .slabs = NULL},
        {.size = 16, .slabs = NULL},
        {.size = 32, .slabs = NULL},
        {.size = 64, .slabs = NULL},
        {.size = 128, .slabs = NULL},
        {.size = 256, .slabs = NULL},
        {.size = 512, .slabs = NULL},
        {.size = 1024, .slabs = NULL},
        {.size = 2048, .slabs = NULL},
        {.size = 4096, .slabs = NULL},
        {.size = 8192, .slabs = NULL},
        {.size = 16384, .slabs = NULL},
};

static int registeredSizeClassesCount = 0;

struct slab* kmalloc_for_size(struct size_class* sizeClass) {
    uint16_t perPage = (4096 / sizeClass->size);
    uint8_t bitmapSize = 8;
    if(perPage > 512) {
      bitmapSize = perPage / 64;
      if(bitmapSize % 8 != 0) {
        bitmapSize = (bitmapSize + 8) & ~(8 - 1);
      }
    }

    struct slab* slab = malloc(sizeof(struct slab));
    memset(slab, 0, sizeof(struct slab));

    slab->total_objects = perPage;
    slab->free_objects = perPage;
    slab->object_size = sizeClass->size;

    if(perPage >= 1) {
      slab->memory = sbrk(4096);
      slab->bitmap = calloc(1, bitmapSize);
    } else {
      slab->memory = sbrk(sizeClass->size);
      slab->bitmap = calloc(1, bitmapSize);
    }

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
    for(int i = 0; i < registeredSizeClassesCount; i++) {
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

            kmalloc_for_size(&registeredSizeClasses[i]);
            slab = slab->next;
            slab->bitmap[0] |= 1 << 0;
            slab->free_objects--;

            return slab->memory;
        }
    }

    int shift = 0;

    while ( shift < 10 )
    {
        if ( defaultSizeClasses[shift].size >= size ) break;
        shift += 1;
    }

    if(defaultSizeClasses[shift].size < size) {
        return NULL; //Size larger than a whole page, please use liballoc or direct mmap!
    }

    if(!defaultSizeClasses[shift].slabs) {
        defaultSizeClasses[shift].slabs = kmalloc_for_size(&defaultSizeClasses[shift]);
    }

    struct slab* slab = defaultSizeClasses[shift].slabs;

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
    slab = defaultSizeClasses[shift].slabs;
    while(slab->next) {
        slab = slab->next;
    }

    kmalloc_for_size(&defaultSizeClasses[shift]);
    slab = slab->next;
    slab->bitmap[0] |= 1 << 0;
    slab->free_objects--;

    return slab->memory;
}

void kfree(void* ptr) {
    if (ptr == NULL) return;

    //Search in default size classes first, because they are cheaper and more widespread
    for (int i = 0; i < DEFAULT_SIZE_CLASSES; i++) {
        if (defaultSizeClasses[i].size > 0) {
            struct slab* slab = defaultSizeClasses[i].slabs;

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
            registeredSizeClassesCount++;
            return;
        }
    }
}

/**
 * Bootstraps the allocator for its own use.
 * The allocator needs 40 bytes for the slab structure and the default size
 */
void alloc_init() {
    uint16_t perPage = (4096 / 40);
    uint16_t bitmapSize = perPage / 64;
    if(bitmapSize % 8 != 0) {
        bitmapSize = (bitmapSize + 8) & ~(8 - 1);
    }

    struct slab* slab = sbrk(4096);

    slab->total_objects = perPage;
    slab->free_objects = perPage;
    slab->object_size = 40;

    slab->memory = (void*)slab;
    slab->bitmap = sbrk(4096); //This memory will be later assigned to our 16 byte default class
    slab->bitmap[0] = 1;

    registeredSizeClasses[0].size = 40;
    registeredSizeClasses[0].slabs = slab;

    struct slab* eightByteSlab = kmalloc(sizeof(struct slab)); //Use our bootstrapped allocator to set up our second slab
    eightByteSlab->total_objects = 512;
    eightByteSlab->free_objects = 512;
    eightByteSlab->object_size = 8;
    eightByteSlab->memory = slab->bitmap;
    eightByteSlab->bitmap = (slab->bitmap + (bitmapSize / 8));
    eightByteSlab->bitmap[0] = 255; //First 8 slots are set

    defaultSizeClasses[0].slabs = eightByteSlab;

    for(int i = 1; i < DEFAULT_SIZE_CLASSES; i++) {
        kmalloc_for_size(&defaultSizeClasses[i]);
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



