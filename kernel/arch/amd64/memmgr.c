//
// Created by Jannik on 10.03.2024.
//
#define BITMAP_SIZE 524288

#include "../../memmgr.h"
#include "../../multiboot.h"
#include "../../terminal.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ADDRESS_TO_PAGE(addr) ((addr >> 12))
#define PAGE_TO_ADDRESS(page) ((page << 12))

#define PAGE_PRESENT 1 << 0
#define PAGE_WRITABLE 1 << 1
#define PAGE_USER 1 << 2
#define PAGE_LARGE 1 << 7

/* Kernel memory base */
#define KERNEL_MEMORY 0xfffffe8000000000ull
/* Defines the offset from virtual memory to physical memory */
#define LOW_MEMORY 0x1ffffffffffull

/**** FUNCTIONS FOR PAGE ID *********/
#define PML4_INDEX(addr) ((addr) >> 39 & 0x1FF)
#define PDP_INDEX(addr) ((addr) >> 30 & 0x1FF)
#define PD_INDEX(addr) ((addr) >> 21 & 0x1FF)
#define PT_INDEX(addr) ((addr) >> 12 & 0x1FF)
#define PAGE_INDEX(addr) ((addr) & 0x1FF)


/********************PHSYICAL MEMORY MANAGEMENT******************/

//Simple bitmap physical memory manager
// 1 = used, 0 = unused
uint32_t memory_map[BITMAP_SIZE];
//Keep the index static so we can allocate up and down
int idx = 0;

/****************************************************************/
/*********************VIRTUAL MEMORY MANAGEMENT******************/
// Identity map of all 512 GB of supported memory, this allows the kernel to fully access the memory addressable using 1 GiB Pages
static uint64_t* IDENTITY_MAP_PD[512] __attribute__((aligned(4096)));
/****************************************************************/

//Static location of the identity boot page map
static uint64_t* PAGE_MAP = 0x1000;
//Base location of the entire memory accessible for kernel
static uint64_t* KERNEL_MEMORY_BASE_PTR = KERNEL_MEMORY;
//Static location of runtime identity page map
static uint64_t* KERNEL_PAGE_MAP = KERNEL_MEMORY + 0x1000;

const static size_t PAGE_SIZE = 4096;
const static uintptr_t KERNEL_ENTRY = 0xffffff0000000000ull;

//Location of the kernel bootstrap, start of the higher level
extern unsigned long _bootstrap_end;

//Actual kernel end, start of the kernel heap
extern unsigned long long _end;

extern void reloadPML();

//The rest after the end. The kernel reserves 4 MB for itself at bootup.
unsigned long kernel_heap_length = 0x1000;

void memmgr_phys_mark_page(int idx) {
    memory_map[idx] = 1;
}

void memmgr_phys_free_page(int idx) {
    memory_map[idx] = 0;
}

uintptr_t kalloc_frame() {
    int i = 0;
    for(;;) {
        if(!memory_map[idx]) {
            //Found free, yay
            memmgr_phys_mark_page(idx);

            return PAGE_TO_ADDRESS(idx);
        }

        if(idx >= BITMAP_SIZE) {
            if(i < BITMAP_SIZE) {
                //We didnt iterate through all
                idx = 0;
                continue;
            } else {
                break;
            }
        } else if(i >= BITMAP_SIZE) {
            break;
        }

        idx++;
        i++;
    }

    //NO FRAME FOUND, WE ARE OFFICIALLY FUCKED (HOW TF DO U USE 64 GB ANYWAY)
    return UINT64_MAX;
}

void kfree_frame(uintptr_t addr) {
    int frame_idx = ADDRESS_TO_PAGE((size_t)addr);

    memmgr_phys_free_page(frame_idx);

    //if idx is still at our frame, put it down, makes search faster, else, idc
    if(idx == frame_idx)
        idx--;
    else
        if(frame_idx < idx)
            idx = frame_idx;
}

/**
 * Returns the current Page Map for this process
 * @return
 */
void* memmgr_get_current_pml4() {
    return KERNEL_PAGE_MAP;
}

/***
 * Returns the physical address for the given memory address in the identity mapping
 * @param virtAddr the virtual addr
 * @return the physical address
 */
void* memmgr_get_from_virtual(uintptr_t virtAddr) {
    return (virtAddr & LOW_MEMORY);
}

/***
 * Returns the identity mapping for the given memory region
 * @param physAddr the physical address
 * @return the virtual memory address for direct access by kernel
 */
void* memmgr_get_from_physical(uintptr_t physAddr) {
    return (physAddr | KERNEL_MEMORY);
}

void* memmgr_create_or_get_page(uintptr_t virtualAddr, int flags) {
    uint64_t INDEX_PML4 = PML4_INDEX(virtualAddr);
    uint64_t INDEX_PDP = PDP_INDEX(virtualAddr);
    uint64_t INDEX_PD = PD_INDEX(virtualAddr);
    uint64_t INDEX_PT = PT_INDEX(virtualAddr);

    uint64_t* pageMap = memmgr_get_current_pml4();
    uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMap[INDEX_PML4]);

    if(pageDirectoryPointer == 0) {
        uintptr_t pdp_frame = kalloc_frame();

        if(pdp_frame == UINT64_MAX) {
            //PANIC
        }

        pageMap[INDEX_PML4] = pdp_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        pageDirectoryPointer = (uint64_t*)(pdp_frame | KERNEL_MEMORY);
    }

    uint64_t* pageDirectory = memmgr_get_from_physical(pageDirectoryPointer[INDEX_PDP]);

    if(pageDirectory == 0) {
        uintptr_t pd_frame = kalloc_frame();
        pageDirectoryPointer[INDEX_PDP] = pd_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        pageDirectory = (uint64_t*)(pd_frame | KERNEL_MEMORY);
    }

    uint64_t* pageTable = memmgr_get_from_physical(pageDirectory[INDEX_PD]);

    if(pageTable == 0) {
        uintptr_t pt_frame = kalloc_frame();
        pageDirectory[INDEX_PDP] = pt_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        pageTable = (uint64_t*)(pt_frame | KERNEL_MEMORY);
    }

    if(pageTable[INDEX_PT] == 0) {
        pageTable[INDEX_PT] = kalloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
    }

    return pageTable[INDEX_PT];
}

void* get_free_page(size_t len) {
    uint64_t* pageMap = memmgr_get_current_pml4();
    uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMap[0]);

    if(pageDirectoryPointer == 0) {
        //No memory???
        return UINT64_MAX;
    }

    uint64_t freeMemory = 0;
    uint64_t baseAddress = 0;

    for(int i = 0; i < 512; i++) {
        uint64_t* pageDirectory = memmgr_get_from_physical(pageDirectoryPointer[i]);

        if(pageDirectory == 0) {
            if(baseAddress == 0) {
                //free page directory means free page, lets calculate offset
                baseAddress = (0x40000000UL * i);
            }

            freeMemory += 0x40000000UL;

            if(freeMemory > len) {
                return baseAddress;
            }

            continue;
        }

        if((uintptr_t)pageDirectory & PAGE_LARGE) {
            //Skip large page
            continue;
        }

        for(int j = 0; j < 512; j++) {
            uint64_t* pageTable = memmgr_get_from_physical(pageDirectory[j]);

            if(pageTable == 0) {
                if(baseAddress == 0) {
                    //free page directory means free page, lets calculate offset
                    baseAddress = (0x40000000UL * i) + (0x200000UL * j);
                }

                freeMemory += 0x200000UL;

                if(freeMemory > len) {
                    return baseAddress;
                }

                continue;
            }

            if((uintptr_t)pageTable & PAGE_LARGE) {
                //Skip large page
                continue;
            }

            for(int k = 0; k < 512; k++) {
                uint64_t page = pageTable[k];

                if(page != 0) {
                    baseAddress = 0;
                    freeMemory = 0;

                    continue;
                }

                if(baseAddress == 0) {
                    //free page directory means free page, lets calculate offset
                    baseAddress = (0x40000000UL * i) + (0x200000UL * j);
                }

                freeMemory += 0x1000UL;

                if(freeMemory > len) {
                    return baseAddress;
                }
            }
        }
    }

    return UINT64_MAX;
}

void memmgr_delete_page(uintptr_t virtualAddr) {
    uint64_t INDEX_PML4 = PML4_INDEX(virtualAddr);
    uint64_t INDEX_PDP = PDP_INDEX(virtualAddr);
    uint64_t INDEX_PD = PD_INDEX(virtualAddr);
    uint64_t INDEX_PT = PT_INDEX(virtualAddr);

    uint64_t* pageMap = memmgr_get_current_pml4();
    uint64_t* pageDirectoryPointer = pageMap[INDEX_PML4];

    if(pageDirectoryPointer == 0 || (uintptr_t)pageDirectoryPointer & PAGE_LARGE) {
        return;
    }

    uint64_t* pageDirectory = pageDirectoryPointer[INDEX_PDP];

    if(pageDirectory == 0 ||(uintptr_t) pageDirectory & PAGE_LARGE) {
        return;
    }

    uint64_t* pageTable = pageDirectory[INDEX_PD];

    if(pageTable == 0 || (uintptr_t)pageTable & PAGE_LARGE) {
        return;
    }

    uintptr_t frame = pageTable[INDEX_PT];

    kfree_frame(frame);

    pageTable[INDEX_PT] = 0;
}

void memmgr_map_frame_to_virtual(uintptr_t frame_addr, uintptr_t virtual_addr) {
    uint64_t INDEX_PML4 = PML4_INDEX(virtual_addr);
    uint64_t INDEX_PDP = PDP_INDEX(virtual_addr);
    uint64_t INDEX_PD = PD_INDEX(virtual_addr);
    uint64_t INDEX_PT = PT_INDEX(virtual_addr);

    uint64_t* pageMap = memmgr_get_current_pml4();
    uint64_t* pageDirectoryPointer = pageMap[INDEX_PML4];

    if(pageDirectoryPointer == 0) {
        uintptr_t pdp_frame = kalloc_frame();

        if(pdp_frame == UINT64_MAX) {
            //PANIC
        }

        pageMap[INDEX_PML4] = pdp_frame | PAGE_PRESENT | PAGE_WRITABLE;

        pageDirectoryPointer = pdp_frame | KERNEL_MEMORY;
    }

    uint64_t* pageDirectory = pageDirectoryPointer[INDEX_PDP];

    if(pageDirectory == 0) {
        uintptr_t pd_frame = kalloc_frame();
        pageDirectoryPointer[INDEX_PDP] = pd_frame | PAGE_PRESENT | PAGE_WRITABLE;

        pageDirectory = pd_frame | KERNEL_MEMORY;
    }

    uint64_t* pageTable = pageDirectory[INDEX_PD];

    if(pageTable == 0) {
        uintptr_t pt_frame = kalloc_frame();
        pageDirectory[INDEX_PDP] = pt_frame | PAGE_PRESENT | PAGE_WRITABLE;

        pageTable = pt_frame | KERNEL_MEMORY;
    }

    pageTable[INDEX_PT] = frame_addr | PAGE_PRESENT | PAGE_WRITABLE;
}

/**
 * Allocates a new memory region, searching for a free place if addr is null or on the address
 * @param addr the desired address
 * @param len the length of the memory region
 * @param is_kernel if is_kernel is set, the length of the map is increased
 */
void* mmap(void* addr, size_t len, bool is_kernel) {
    //Page-align
    if(!((uintptr_t)addr & (PAGE_SIZE - 1)) && addr != 0) {
        addr = (uintptr_t)((uintptr_t)addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    }

    if(addr == 0) {
        if(is_kernel) {
            uintptr_t start_addr = (uintptr_t)(&_end);
            if(!((uintptr_t)start_addr & (PAGE_SIZE - 1))) {
                start_addr = (uintptr_t)((uintptr_t)start_addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
            }

            size_t count = len / 4096;
            if(len % 4096 != 0) {
                count++;
            }

            for(size_t i = 1; i < count+1; i++) {
                memmgr_create_or_get_page(start_addr + 0x1000 * i, PAGE_USER);
            }

            return start_addr;
        } else {
            //Search new space
            uintptr_t start_addr = get_free_page(len);

            size_t count = len / 4096;
            if(len % 4096 != 0) {
                count++;
            }

            for(size_t i = 1; i < count+1; i++) {
                memmgr_create_or_get_page(start_addr * i, PAGE_USER);
            }

            return start_addr;
        }
    } else {
        size_t count = len / 4096;
        if(len % 4096 != 0) {
            count++;
        }

        for(size_t i = 1; i < count+1; i++) {
            memmgr_create_or_get_page((uintptr_t)addr * i, PAGE_USER);
        }

        return addr;
    }
}

void munmap(void* addr, size_t len) {
    //Page-align
    if(!((size_t)addr & (PAGE_SIZE - 1))) {
        addr = (size_t)((size_t*)addr + PAGE_SIZE) & (PAGE_SIZE - 1);
    }

    size_t count = len / 4096;
    if(len % 4096 != 0) {
        count++;
    }

    for(size_t i = 1; i < count+1; i++) {
        memmgr_delete_page((uintptr_t)addr * i);
    }
}

/***
 * Sets the size of the kernel heap
 * The kernel heap always starts in virtual memory after the kernel binary
 * @param len the increment
 */
void sbrk(intptr_t len) {
    if(len > 0) {
        //Map new kernel space
        mmap(0, kernel_heap_length + len, true);
    } else {
        //Unmap kernel space
        munmap(&_end + KERNEL_ENTRY + kernel_heap_length - len, len);
    }

    kernel_heap_length += len;
}

void brk(size_t len) {
    size_t prev = kernel_heap_length;

    kernel_heap_length = len;

    if(prev > len) {
        munmap(&_end + KERNEL_ENTRY + kernel_heap_length, prev - kernel_heap_length);
    } else {
        mmap(0, len, true);
    }
}

size_t get_kernel_heap_length() {
    return kernel_heap_length;
}

//Dumps the internal bitmap
void memmgr_dump() {
    int startAddr = -1;
    int endAddr = -1;
    int size = 0;

    for(int i = 0; i < BITMAP_SIZE; i++) {
        if(memory_map[i]) {
            if(startAddr == -1) {
                startAddr = PAGE_TO_ADDRESS(i);
            }

            size += 0x1000;
        } else {
            if(startAddr != -1) {
                endAddr = PAGE_TO_ADDRESS(i);

                printf("MEM START: %x, MEM END: %x, MEM SIZE: %d\n", startAddr, endAddr, size);

                startAddr = -1;
                endAddr = -1;
                size = 0;
            }
        }
    }
}

void memmgr_init(multiboot_info_t* info) {
    printf("Mem lower: %d\n", info->mem_lower);
    printf("Mem upper: %d\n", info->mem_upper);
    //printf("info loc: 0x%x\n", info);

    multiboot_memory_map_t* mmap;

    for (mmap = (multiboot_memory_map_t *) info->mmap_addr;
         (unsigned long) mmap < info->mmap_addr + info->mmap_length;
         mmap = (multiboot_memory_map_t *) ((unsigned long) mmap
                                            + mmap->size + sizeof (mmap->size))) {
        //printf("Found mmap, analysing\n");
        //printf("Mmap start: 0x%x, Mmap end: 0x%x, Mmap size: %d, Mmap type: %d \n", mmap->addr, mmap->addr + mmap->len, mmap->len, mmap->type);

        if(mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            for(int i = mmap->addr; i < mmap->addr + mmap->len; i += 0x1000) {
                memmgr_phys_mark_page(ADDRESS_TO_PAGE(i));
            }
        }
    }

    //Map kernel low memory
    for(int i = 0x1000; i < 0x401000; i += 0x1000) {
        memmgr_phys_mark_page(ADDRESS_TO_PAGE(i));
    }

    //Map kernel heap

    uint64_t* IDENTITY_MAP_PD_TEMP = (uintptr_t)IDENTITY_MAP_PD - 0xffffff0000000000ull;

    printf("IDENTITY MAP LOC: 0x%x\n", IDENTITY_MAP_PD_TEMP);

    for(uint64_t i = 0; i < 512; i++) {
        IDENTITY_MAP_PD_TEMP[i] = 0;
    }

    //Identity map 512 GiB of memory to high memory
    for(uint64_t i = 0; i < 512; i++) {
        IDENTITY_MAP_PD_TEMP[i] = i * 0x40000000ULL | PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE;
    }

    PAGE_MAP[509] = (uintptr_t)IDENTITY_MAP_PD_TEMP | PAGE_PRESENT | PAGE_WRITABLE;

    kernel_heap_length = 0x1000;

    //Increase kernel heap to 2 MB
    brk(0x200000);

    reloadPML();

    printf("end\n");

    //memmgr_dump();
}
