//
// Created by Jannik on 10.03.2024.
//

#ifndef NIGHTOS_MEMMGR_H
#define NIGHTOS_MEMMGR_H

#include "multiboot2.h"
#include <stdint.h>
#include <stdbool.h>
#include "alloc.h"

#define MAP_PRIVATE 2

#define PROT_EXEC 1<<0
#define PROT_READ 1<<1
#define PROT_WRITE 1<<2
#define PROT_NONE 0

#define CHECK_PTR(ptr) (ptr < 0xfffffe0000000000ull && memmgr_check_user(ptr))

#define ADDRESS_TO_PAGE(addr) ((addr >> 12))
#define PAGE_TO_ADDRESS(page) ((page << 12))

#define FLAG_WB 0x0
#define FLAG_WT 0x8
#define FLAG_UCMINUS 0x10
#define FLAG_UC 0x18
#define FLAG_WP 0x80
#define FLAG_WC 0x88


void memmgr_init(struct multiboot_tag_mmap* info, uintptr_t kernel_end);

uintptr_t kalloc_frame();
void kfree_frame(uintptr_t addr);

void memmgr_phys_mark_page(int idx);
void memmgr_phys_free_page(int idx);

void memmgr_map_frame_to_virtual(uintptr_t frame_addr, uintptr_t virtual_addr, uintptr_t flags);
/**
 * Deallocates a memory region
 * @param addr the desired address
 * @param len the length of the memory region
 */
void munmap(void* addr, size_t len);
/**
 * Allocates a new memory region, searching for a free place if addr is null or on the address
 * @param addr the desired address
 * @param len the length of the memory region
 * @param is_kernel if is_kernel is set, the length of the map is increased
 */
void* mmap(void* addr, size_t len, bool is_kernel);

void memmgr_clone_page_map(uint64_t* pageMapOld, uint64_t* pageMapNew);
/**
 * Returns the current Page Map for the kernel process
 * @return
 */
void* memmgr_get_current_pml4();
void* memmgr_create_or_get_page(uintptr_t virtualAddr, int flags, int create);
bool memmgr_change_flags(uintptr_t virt, int flags);
bool memmgr_change_flags_bulk(uintptr_t virt, int pages, int flags);
void memmgr_clear_page_map(uintptr_t pageMap);

void* memmgr_map_mmio(uintptr_t addr, size_t len, int flags, bool is_kernel);
void* memmgr_get_mmio(uintptr_t addr);

void load_page_map(uintptr_t pageMap);

/***
 * Returns the physical address for the given memory address in the identity mapping
 * @param virtAddr the virtual addr
 * @return the physical address
 */
void* memmgr_get_from_virtual(uintptr_t virtAddr);
/***
 * Returns the identity mapping for the given memory region
 * @param physAddr the physical address
 * @return the virtual memory address for direct access by kernel
 */
void* memmgr_get_from_physical(uintptr_t physAddr);
/***
 * Returns the physical address for the given memory address
 * @param virtaddr the virtual address
 * @return the mapped address
 */
void* memmgr_get_page_physical(uintptr_t virtaddr);

void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memset(void*, int, size_t);

/***
 * Increments the size of the kernel heap
 * The kernel heap always starts in virtual memory after the kernel binary
 * @param len the increment
 */
void* sbrk(intptr_t len);
/***
 * Sets the size of the kernel heap
 * The kernel heap always starts in virtual memory after the kernel binary
 * @param len the increment
 */
void* brk(size_t len);

bool memmgr_check_user(uintptr_t addr);

size_t get_heap_length();

#endif //NIGHTOS_MEMMGR_H
