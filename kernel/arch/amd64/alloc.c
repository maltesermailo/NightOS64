//
// Created by Jannik on 17.03.2024.
//
#include "../../memmgr.h"

/*extern unsigned long _end;

static unsigned long KERNEL_HEAP_START_PTR;
static free_memory_block_t* free_memory_map;

void* memset(void* bufptr, int value, size_t size) {
    unsigned char* buf = (unsigned char*) bufptr;
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char) value;
    return bufptr;
}

void* kmalloc(size_t size) {
    if(size > free_memory_map->len) {
        //Not enough memory, we need to allocate more.
    }

    memory_alloc_t* mem_block = free_memory_map->base;
    memset(mem_block, 0, sizeof(memory_alloc_t));

    mem_block->magic = MEM_BLOCK_MAGIC;
    mem_block->len = size;

    free_memory_map->base = free_memory_map->base + size;
    free_memory_map->len = free_memory_map->len - size;

    void* addr = mem_block + sizeof(memory_alloc_t);

    return addr;
}

void kfree(void* ptr) {
    //mem_alloc_t is at ptr minus sizeof(memory_alloc_t)

    memory_alloc_t* mem_block = (ptr - sizeof(memory_alloc_t));

    if(mem_block->magic != MEM_BLOCK_MAGIC) {
        //INVALID POINTER
        return;
    }
}

void init_kernel_heap() {
    KERNEL_HEAP_START_PTR = _end;

    /*if(!(KERNEL_HEAP_START_PTR & (PAGE_SIZE - 1))) {
        KERNEL_HEAP_START_PTR = (KERNEL_HEAP_START_PTR + PAGE_SIZE) & (PAGE_SIZE - 1);
    }*/

    //Init free_memory_block list
    /*memset(KERNEL_HEAP_START_PTR, 0, sizeof(free_memory_block_t));
    free_memory_map = KERNEL_HEAP_START_PTR;
    free_memory_map->base = KERNEL_HEAP_START_PTR + sizeof(free_memory_block_t);
    free_memory_map->len = KERNEL_HEAP_START_PTR + 0x200000 - (KERNEL_HEAP_START_PTR - _end) - sizeof(free_memory_block_t);
}*/

const static int page_size = 0x1000;

int liballoc_lock()
{
    //TODO: Once threads are implemented
    return 0;
}

int liballoc_unlock()
{
    //TODO: Once threads are implemented
    return 0;
}

void* liballoc_alloc( int pages )
{
    unsigned int size = pages * page_size;

    char *p2 = (char*) mmap(0, size, 1);
    if ( p2 == -1) return NULL;

    return p2;
}

int liballoc_free( void* ptr, int pages )
{
    munmap( ptr, pages * page_size );
    return 0;
}



