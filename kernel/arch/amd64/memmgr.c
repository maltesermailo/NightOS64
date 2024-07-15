//
// Created by Jannik on 10.03.2024.
//
#define BITMAP_SIZE 524288

#include "../../memmgr.h"
#include "../../multiboot2.h"
#include "../../terminal.h"
#include "../../proc/process.h"
#include "../../lock.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ADDRESS_TO_PAGE(addr) ((addr >> 12))
#define PAGE_TO_ADDRESS(page) ((page << 12))

#define PAGE_PRESENT 1 << 0
#define PAGE_WRITABLE 1 << 1
#define PAGE_USER 1 << 2
#define PAGE_LARGE 1 << 7
#define PAGE_NOCACHE 1 << 4
#define PAGE_WRITE_THROUGH 1 << 3

/* Kernel memory base */
#define KERNEL_MEMORY 0xfffffe8000000000ull
#define MMIO_MEMORY   0xfffffe0000000000ull
/* Defines the offset from virtual memory to physical memory */
#define LOW_MEMORY 0x7fffffffffull

/**** FUNCTIONS FOR PAGE ID *********/
#define PML4_INDEX(addr) ((addr) >> 39 & 0x1FF)
#define PDP_INDEX(addr) ((addr) >> 30 & 0x1FF)
#define PD_INDEX(addr) ((addr) >> 21 & 0x1FF)
#define PT_INDEX(addr) ((addr) >> 12 & 0x1FF)
#define PAGE_INDEX(addr) ((addr) & 0x1FF)

#define PAGE_MASK 0xfffffffffffff000ull

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
static unsigned long long _end;

extern void reloadPML();

//The rest after the end. The kernel reserves 4 MB for itself at bootup.
unsigned long kernel_heap_length = 0x1000;

static spin_t PHYS_MEM_LOCK = ATOMIC_FLAG_INIT;
static spin_t VIRT_MEM_LOCK = ATOMIC_FLAG_INIT;

void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
    unsigned char* dst = (unsigned char*) dstptr;
    const unsigned char* src = (const unsigned char*) srcptr;
    for (size_t i = 0; i < size; i++)
        dst[i] = src[i];
    return dstptr;
}

void* memset(void* bufptr, int value, size_t size) {
    unsigned char* buf = (unsigned char*) bufptr;
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char) value;
    return bufptr;
}


void memmgr_phys_mark_page(int idx) {
    if(idx >= BITMAP_SIZE) {
        return;
    }

    memory_map[idx] = 1;
}

void memmgr_phys_free_page(int idx) {
    if(idx >= BITMAP_SIZE) {
        return;
    }

    memory_map[idx] = 0;
}

uintptr_t kalloc_frame() {
    spin_lock(&PHYS_MEM_LOCK);
    int idx = 0;
    for(;;) {
        if(!memory_map[idx]) {
            //Found free, yay
            memmgr_phys_mark_page(idx);

            spin_unlock(&PHYS_MEM_LOCK);
            return PAGE_TO_ADDRESS(idx);
        }

        if(idx >= BITMAP_SIZE) {
            if(idx < BITMAP_SIZE) {
                //We didnt iterate through all
                idx = 0;
                continue;
            } else {
                break;
            }
        } else if(idx >= BITMAP_SIZE) {
            break;
        }

        idx++;
    }


    spin_unlock(&PHYS_MEM_LOCK);
    //NO FRAME FOUND, WE ARE OFFICIALLY FUCKED (HOW TF DO U USE 64 GB ANYWAY)
    return UINT64_MAX;
}

void kfree_frame(uintptr_t addr) {
    spin_lock(&PHYS_MEM_LOCK);
    int frame_idx = ADDRESS_TO_PAGE((size_t)addr);

    memmgr_phys_free_page(frame_idx);

    //if idx is still at our frame, put it down, makes search faster, else, idc
    if(idx == frame_idx)
        idx--;
    else
        if(frame_idx < idx)
            idx = frame_idx;

    spin_unlock(&PHYS_MEM_LOCK);
}

/**
 * Returns the current Page Map for the kernel process
 * @return
 */
void* memmgr_get_current_pml4() {
    return process_get_current_pml();
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
 * Returns the physical address for the given memory address
 * @param virtaddr the virtual address
 * @return the mapped address
 */
void* memmgr_get_page_physical(uintptr_t virtaddr) {
    void* physAddr = memmgr_create_or_get_page(virtaddr, 0, 0);

    if(physAddr == 0) {
        return 0;
    }

    uintptr_t offset = virtaddr % 4096;

    return physAddr + offset;
}

/***
 * Returns the identity mapping for the given memory region
 * @param physAddr the physical address
 * @return the virtual memory address for direct access by kernel
 */
void* memmgr_get_from_physical(uintptr_t physAddr) {
    return (physAddr | KERNEL_MEMORY);
}

void* memmgr_get_mmio(uintptr_t physAddr) {
    return (physAddr | MMIO_MEMORY);
}

extern void load_page_map0(uintptr_t pageMap);

void load_page_map(uintptr_t pageMap) {
    if(pageMap == 0) {
        pageMap = 0x1000;
    }

    asm volatile (
            "movq %0, %%cr3"
            : : "r"((uintptr_t)pageMap));
}

void* memmgr_create_or_get_page(uintptr_t virtualAddr, int flags, int create) {
    uint64_t INDEX_PML4 = PML4_INDEX(virtualAddr);
    uint64_t INDEX_PDP = PDP_INDEX(virtualAddr);
    uint64_t INDEX_PD = PD_INDEX(virtualAddr);
    uint64_t INDEX_PT = PT_INDEX(virtualAddr);

    //printf("Virtual addr: 0x%x\n", virtualAddr);
    //printf("INDICES: 0x%x, 0x%x, 0x%x, 0x%x\n", INDEX_PML4, INDEX_PDP, INDEX_PD, INDEX_PT);

    uint64_t* pageMap = memmgr_get_from_physical(memmgr_get_current_pml4());
    uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMap[INDEX_PML4] & PAGE_MASK);

    if(pageMap[INDEX_PML4] == 0) {
        if(!create) {
            return NULL;
        }

        uintptr_t pdp_frame = kalloc_frame();

        if(pdp_frame == UINT64_MAX) {
            //PANIC
        }

        pageMap[INDEX_PML4] = pdp_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        pageDirectoryPointer = (uint64_t*)(pdp_frame | KERNEL_MEMORY);
    }

    uint64_t* pageDirectory = memmgr_get_from_physical(pageDirectoryPointer[INDEX_PDP] & PAGE_MASK);

    if(pageDirectoryPointer[INDEX_PDP] == 0) {
        if(!create) return NULL;

        uintptr_t pd_frame = kalloc_frame();
        pageDirectoryPointer[INDEX_PDP] = pd_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        pageDirectory = (uint64_t*)(pd_frame | KERNEL_MEMORY);
    }

    uint64_t* pageTable = memmgr_get_from_physical(pageDirectory[INDEX_PD] & PAGE_MASK);

    if(pageDirectory[INDEX_PD] == 0) {
        if(!create) return NULL;
        uintptr_t pt_frame = kalloc_frame();
        pageDirectory[INDEX_PD] = pt_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        printf("Creating page table at 0x%x with index 0x%x in PD 0x%x\n", pt_frame, INDEX_PD, pageDirectory);

        pageTable = (uint64_t*)(pt_frame | KERNEL_MEMORY);
    }

    if(pageTable[INDEX_PT] == 0) {
        if(!create) return NULL;
        //printf("Creating new page\n");
        pageTable[INDEX_PT] = kalloc_frame() | PAGE_PRESENT | PAGE_WRITABLE | flags;
    }

    return (void *) (pageTable[INDEX_PT] & PAGE_MASK);
}

void* get_free_page(size_t len) {
    uint64_t* pageMap = memmgr_get_from_physical(memmgr_get_current_pml4());
    uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMap[0] & PAGE_MASK);

    //printf("Page map loc: 0x%x\n", pageMap);
    //printf("Page directory pointer: 0x%x\n", pageDirectoryPointer);

    if(pageDirectoryPointer == 0) {
        //No memory???
        return UINT64_MAX;
    }

    uint64_t freeMemory = 0;
    uint64_t baseAddress = 0;

    for(int i = 0; i < 512; i++) {
        uint64_t* pageDirectory = memmgr_get_from_physical(pageDirectoryPointer[i] & PAGE_MASK);
        //printf("Page directory: %d, 0x%x\n", i, pageDirectory);

        if(pageDirectoryPointer[i] & PAGE_LARGE) {
            //Skip large page
            continue;
        }

        if(pageDirectoryPointer[i] == 0) {
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


        if(!(pageDirectoryPointer[i] & PAGE_USER)) {
            //No user page directory, skip
            continue;
        }

        for(int j = 0; j < 512; j++) {
            uint64_t* pageTable = memmgr_get_from_physical(pageDirectory[j] & PAGE_MASK);
            //printf("Page table: %d: 0x%x\n", j, pageTable);

            if((uintptr_t)pageTable & PAGE_LARGE) {
                //Skip large page
                continue;
            }

            if(pageDirectory[j] == 0) {
                if(baseAddress == 0) {
                    //free page table means free page, lets calculate offset
                    baseAddress = (0x40000000UL * i) + (0x200000UL * j);
                }

                freeMemory += 0x200000UL;

                if(freeMemory > len) {
                    printf("cool: 0x%x\n", baseAddress);
                    return baseAddress;
                }

                continue;
            }

            if(!(pageDirectory[j] & PAGE_USER)) {
                //No user page table, skip
                continue;
            }

            for(int k = 0; k < 512; k++) {
                uint64_t page = pageTable[k];

                if(page != 0 && pageTable[k] != KERNEL_MEMORY) {
                    baseAddress = 0;
                    freeMemory = 0;

                    continue;
                }

                if(baseAddress == 0) {
                    //free page directory means free page, lets calculate offset
                    baseAddress = (0x40000000UL * i) + (0x200000UL * j) +(0x1000 * k);
                }

                freeMemory += 0x1000UL;

                if(freeMemory > len) {
                    printf("cool2: 0x%x\n", baseAddress);
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
    uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMap[INDEX_PML4] & PAGE_MASK);

    if(pageDirectoryPointer == 0 || (uintptr_t)pageMap[INDEX_PML4] & PAGE_LARGE) {
        return;
    }

    uint64_t* pageDirectory = memmgr_get_from_physical(pageDirectoryPointer[INDEX_PDP] & PAGE_MASK);

    if(pageDirectory == 0 ||(uintptr_t) pageDirectoryPointer[INDEX_PDP] & PAGE_LARGE) {
        return;
    }

    uint64_t* pageTable = memmgr_get_from_physical(pageDirectory[INDEX_PD] & PAGE_MASK);

    if(pageTable == 0 || (uintptr_t)pageDirectory[INDEX_PD] & PAGE_LARGE) {
        return;
    }

    uintptr_t frame = pageTable[INDEX_PT];

    kfree_frame(frame);

    pageTable[INDEX_PT] = 0;
}

void memmgr_map_frame_to_virtual(uintptr_t frame_addr, uintptr_t virtual_addr, uintptr_t flags) {
    uint64_t INDEX_PML4 = PML4_INDEX(virtual_addr);
    uint64_t INDEX_PDP = PDP_INDEX(virtual_addr);
    uint64_t INDEX_PD = PD_INDEX(virtual_addr);
    uint64_t INDEX_PT = PT_INDEX(virtual_addr);

    uint64_t* pageMap = memmgr_get_current_pml4();
    uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMap[INDEX_PML4] & PAGE_MASK);

    if(pageMap[INDEX_PML4] == 0) {
        uintptr_t pdp_frame = kalloc_frame();

        if(pdp_frame == UINT64_MAX) {
            //PANIC
        }

        pageMap[INDEX_PML4] = pdp_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        pageDirectoryPointer = (uint64_t*)(pdp_frame | KERNEL_MEMORY);
    }

    uint64_t* pageDirectory = memmgr_get_from_physical(pageDirectoryPointer[INDEX_PDP] & PAGE_MASK);

    if(pageDirectoryPointer[INDEX_PDP] == 0) {
        uintptr_t pd_frame = kalloc_frame();
        pageDirectoryPointer[INDEX_PDP] = pd_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        pageDirectory = (uint64_t*)(pd_frame | KERNEL_MEMORY);
    }

    uint64_t* pageTable = memmgr_get_from_physical(pageDirectory[INDEX_PD] & PAGE_MASK);

    if(pageDirectory[INDEX_PD] == 0) {
        uintptr_t pt_frame = kalloc_frame();
        pageDirectory[INDEX_PD] = pt_frame | PAGE_PRESENT | PAGE_WRITABLE | flags;

        //printf("Creating page table at 0x%x with index 0x%x in PD 0x%x\n", pt_frame, INDEX_PD, pageDirectory);

        pageTable = (uint64_t*)(pt_frame | KERNEL_MEMORY);
    }

    pageTable[INDEX_PT] = frame_addr | PAGE_PRESENT | PAGE_WRITABLE | flags;
}

/***
 * Invalides the tlb at address
 * @param addr the address
 */
void memmgr_reload(uintptr_t addr) {
    asm volatile (
            "invlpg (%0)"
            : : "r"(addr));
}

void* memmgr_map_mmio(uintptr_t addr, size_t len, bool is_kernel) {
    if(addr != 0 && (((uintptr_t)addr % PAGE_SIZE) != 0)) {
        addr = (uintptr_t)((uintptr_t)addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    }

    int flags = PAGE_WRITABLE | PAGE_NOCACHE;
    if(!is_kernel) flags |= PAGE_USER;

    uintptr_t start_addr = MMIO_MEMORY + addr;

    size_t count = len / 4096;
    if(len % 4096 != 0) {
        count++;
    }

    for(size_t i = 0; i < count+1; i++) {
        memmgr_map_frame_to_virtual(addr + 0x1000 * i, start_addr + 0x1000 * i, flags);
        memmgr_reload(start_addr + 0x1000 * i);
        printf("Mapped %d\n", i);
    }

    return start_addr;
}

/**
 * Allocates a new memory region, searching for a free place if addr is null or on the address
 * @param addr the desired address
 * @param len the length of the memory region
 * @param is_kernel if is_kernel is set, the length of the map is increased
 */
void* mmap(void* addr, size_t len, bool is_kernel) {
    //Page-align
    if(addr != 0 && (((uintptr_t)addr % PAGE_SIZE) != 0)) {
        addr = (uintptr_t)((uintptr_t)addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    }

    if(addr == 0) {
        if(is_kernel) {
            uintptr_t start_addr = (uintptr_t)(_end);
            if(!((uintptr_t)start_addr & (PAGE_SIZE - 1))) {
                start_addr = (uintptr_t)((uintptr_t)start_addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
            }

            size_t count = len / 4096;
            if(len % 4096 != 0) {
                count++;
            }

            for(size_t i = 0; i < count; i++) {
                memmgr_create_or_get_page(start_addr + 0x1000 * i, 0, 1);
                memmgr_reload(start_addr + 0x1000 * i);
            }

            return start_addr;
        } else {
            printf("Get next free page...\n");
            //Search new space
            uintptr_t start_addr = get_free_page(len);

            size_t count = len / 4096;
            if(len % 4096 != 0) {
                count++;
            }

            printf("Start addr: 0x%x, Count: %d\n", start_addr, count);

            for(size_t i = 0; i < count; i++) {
                memmgr_create_or_get_page(start_addr + 0x1000 * i, PAGE_USER, 1);
                memmgr_reload(start_addr + 0x1000 * i);
            }

            return start_addr;
        }
    } else {
        size_t count = len / 4096;
        if(len % 4096 != 0) {
            count++;
        }

        for(size_t i = 0; i < count; i++) {
            memmgr_create_or_get_page((uintptr_t)addr + 0x1000 * i, is_kernel ? 0 : PAGE_USER, 1);
            memmgr_reload(addr + 0x1000 * i);
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
        memmgr_delete_page((uintptr_t)addr + 0x1000 * i);
        memmgr_reload(addr + 0x1000 * i);
    }
}

void memmgr_clear_page_map(uintptr_t pageMap) {
    uint64_t* pageMapVirt = memmgr_get_from_physical(pageMap);
    uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMapVirt[0] & PAGE_MASK);

    for(int i = 0; i < 512; i++) {
        if(!(pageDirectoryPointer[i] & PAGE_PRESENT)) {
            continue;
        }

        if(pageDirectoryPointer[i] & PAGE_LARGE) {
            continue;
        }

        uint64_t* pageDirectory = memmgr_get_from_physical(pageDirectoryPointer[i] & PAGE_MASK);

        for(int j = 0; j < 512; j++) {
            if(!(pageDirectory[j] & PAGE_PRESENT)) {
                continue;
            }

            if(pageDirectory[j] & PAGE_LARGE) {
                continue;
            }

            kfree_frame(pageDirectory[j]);
        }

        kfree_frame(pageDirectoryPointer[i]);
    }

    kfree_frame(pageMapVirt[0]);
    kfree_frame((uintptr_t) pageMap);
}

/***
 * Sets the size of the kernel heap
 * The kernel heap always starts in virtual memory after the kernel binary
 * @param len the increment
 */
void* sbrk(intptr_t len) {
    spin_lock(&VIRT_MEM_LOCK);

    void* ptr = 0;

    if(len > 0) {
        //Map new kernel space
        ptr = mmap(0, kernel_heap_length + len, true) + kernel_heap_length;
    } else {
        //Unmap kernel space
        munmap(&_end + KERNEL_ENTRY + kernel_heap_length + len, len);
        ptr = (&_end + KERNEL_ENTRY + kernel_heap_length + len);
    }

    kernel_heap_length += len;
    spin_unlock(&VIRT_MEM_LOCK);

    return ptr;
}

void* brk(size_t len) {
    spin_lock(&VIRT_MEM_LOCK);
    size_t prev = kernel_heap_length;

    kernel_heap_length = len;

    if(prev > len) {
        munmap(&_end + KERNEL_ENTRY + kernel_heap_length, prev - kernel_heap_length);
    } else {
        mmap(0, len, true);
    }

    uintptr_t start_addr = (uintptr_t)(&_end);
    if(!((uintptr_t)start_addr & (PAGE_SIZE - 1))) {
        start_addr = (uintptr_t)((uintptr_t)start_addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    }

    spin_unlock(&VIRT_MEM_LOCK);

    return (void*)start_addr + kernel_heap_length;
}

void memmgr_clone_page_map(uint64_t* pageMapOld, uint64_t* pageMapNew) {
    memset(pageMapNew, 0, 4096);

    for(int i = 0; i < 511; i++) {
        uint64_t* pageDirectoryPointerOld = memmgr_get_from_physical(pageMapOld[i] & PAGE_MASK);

        if(pageMapOld[i] & PAGE_LARGE) {
            //Large page, just copy
            printf("Copying PDPT %d\n", i);
            printf("Copying large pdpt\n");

            pageMapNew[i] = pageMapOld[i];
            continue;
        }

        if(pageMapOld[i] == 0) {
            continue;
        }

        printf("Copying PDPT %d\n", i);

        if(i == 508 || i == 509 || i == 510) {
            //Kernel space, just copy old PDP
            pageMapNew[i] = pageMapOld[i];
            continue;
        }

        uintptr_t pdp_frame = kalloc_frame();

        pageMapNew[i] = (uintptr_t) pdp_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        uint64_t* pageDirectoryPointer = (uint64_t*) memmgr_get_from_physical(pdp_frame);
        memset(pageDirectoryPointer, 0, 4096);

        for(int j = 0; j < 512; j++) {
            uint64_t* pageDirectoryOld = memmgr_get_from_physical(pageDirectoryPointerOld[j] & PAGE_MASK);

            if(pageDirectoryPointerOld[j] & PAGE_LARGE) {
                //Large page, just copy
                printf("Copying PD %d-%d\n", i, j);
                printf("Copying large pd\n");
                pageDirectoryPointer[j] = pageDirectoryPointerOld[j];
                continue;
            }

            if(pageDirectoryPointerOld[j] == 0) {
                continue;
            }

            printf("Copying PD %d-%d\n", i, j);

            uintptr_t pd_frame = kalloc_frame();
            pageDirectoryPointer[j] = pd_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

            uint64_t* pageDirectory = (uint64_t*) memmgr_get_from_physical(pd_frame);
            memset(pageDirectory, 0, 4096);

            for(int k = 0; k < 512; k++) {
                uint64_t* pageTableOld = memmgr_get_from_physical(pageDirectoryOld[k] & PAGE_MASK);

                if(pageDirectoryOld[k] & PAGE_LARGE) {
                    //Large page, just copy
                    printf("Copying PT %d-%d-%d\n", i, j, k);
                    printf("Copying large pt\n");
                    pageDirectory[k] = pageDirectoryOld[k];
                    continue;
                }

                if(pageDirectoryOld[k] == 0) {
                    continue;
                }

                printf("Copying PT %d-%d-%d\n", i, j, k);

                uintptr_t pt_frame = kalloc_frame();
                pageDirectory[k] = pt_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

                uint64_t* pageTable = (uint64_t*) memmgr_get_from_physical(pt_frame);
                memset(pageTable, 0, 4096);

                for(int l = 0; l < 512; l++) {
                    uintptr_t page = pageTableOld[l];

                    if(page == 0) {
                        continue;
                    }

                    printf("Copying page %d-%d-%d-%d\n", i, j, k, l);

                    if(page & PAGE_USER) {
                        uintptr_t page_frame = kalloc_frame();
                        pageTable[l] = page_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

                        //Copy page
                        memcpy(memmgr_get_from_physical(page_frame), memmgr_get_from_physical(page & PAGE_MASK), 4096);
                    } else {
                        pageTable[l] = page;
                    }
                }
            }
        }
    }

    pageMapNew[511] = memmgr_get_from_virtual(pageMapNew);

    //Dump new page map
    for(int i = 0; i < 512; i++) {
        uint64_t* pageDirectoryPointer = memmgr_get_from_physical(pageMapNew[i] & PAGE_MASK);

        //printf("PML4 entry is 0x%x\n", pageDirectoryPointer);
        if(pageDirectoryPointer != KERNEL_MEMORY) {
            printf("PML4 Entry #%d is set.\n", i);
        }
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

void memmgr_init(struct multiboot_tag_mmap* tag, uintptr_t kernel_end) {
    //printf("info loc: 0x%x\n", info);
    _end = kernel_end | 0xffffff0000000000ull;

    multiboot_memory_map_t* mmap;

    spin_unlock(&PHYS_MEM_LOCK);

    for (mmap = ((struct multiboot_tag_mmap *) tag)->entries;
         (multiboot_uint8_t *) mmap
         < (multiboot_uint8_t *) tag + tag->size;
         mmap = (multiboot_memory_map_t *)
                 ((unsigned long) mmap
                  + ((struct multiboot_tag_mmap *) tag)->entry_size)) {
        printf("Mmap start: 0x%x, Mmap end: 0x%x, Mmap size: %d, Mmap type: %d \n", mmap->addr, mmap->addr + mmap->len, mmap->len, mmap->type);

        if(mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            for(unsigned long i = mmap->addr; i < mmap->addr + mmap->len; i += 0x1000) {
                memmgr_phys_mark_page(ADDRESS_TO_PAGE(i));
            }
        }
    }

    //Map kernel low memory
    for(int i = 0x0; i < 0x801000; i += 0x1000) {
        memmgr_phys_mark_page(ADDRESS_TO_PAGE(i));
    }
    spin_unlock(&PHYS_MEM_LOCK);

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

    kernel_heap_length = 0x0;

    reloadPML();

    printf("end\n");

    //memmgr_dump();
}
