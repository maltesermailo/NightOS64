//
// Created by Jannik on 10.03.2024.
//

#ifndef NIGHTOS_MEMMGR_H
#define NIGHTOS_MEMMGR_H

#include "multiboot2.h"
#include <stdint.h>
#include <stdbool.h>
#include "alloc.h"


void memmgr_init(struct multiboot_tag_mmap* info);

uintptr_t kalloc_frame();
void kfree_frame(uintptr_t addr);

void memmgr_map_frame_to_virtual(uintptr_t frame_addr, uintptr_t virtual_addr, uintptr_t flags);
void munmap(void* addr, size_t len);
void* mmap(void* addr, size_t len, bool is_kernel);

void memmgr_clone_page_map(uint64_t* pageMapOld, uint64_t* pageMapNew);
void* memmgr_get_current_pml4();
void* memmgr_create_or_get_page(uintptr_t virtualAddr, int flags);
void memmgr_clear_page_map(uintptr_t pageMap);

void* memmgr_map_mmio(uintptr_t addr, size_t len, bool is_kernel);
void* memmgr_get_mmio(uintptr_t addr);

void load_page_map(uintptr_t pageMap);

void* memmgr_get_from_virtual(uintptr_t virtAddr);
void* memmgr_get_from_physical(uintptr_t physAddr);

        void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memset(void*, int, size_t);

void* sbrk(intptr_t len);
void* brk(size_t len);

/***
 * Returns the identity mapping for the given memory region
 * @param physAddr the physical address
 * @return the virtual memory address for direct access by kernel
 */
void* memmgr_get_from_physical(uintptr_t physAddr);

size_t get_heap_length();

#endif //NIGHTOS_MEMMGR_H
