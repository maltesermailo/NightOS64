//
// Created by Jannik on 10.03.2024.
//

#ifndef NIGHTOS_MEMMGR_H
#define NIGHTOS_MEMMGR_H

#include "multiboot.h"
#include <stdint.h>
#include <stdbool.h>
#include "alloc.h"


void memmgr_init(multiboot_info_t* info);

uintptr_t kalloc_frame();
void kfree_frame(uintptr_t addr);

void memmgr_map_frame_to_virtual(uintptr_t frame_addr, uintptr_t virtual_addr);
void munmap(void* addr, size_t len);
void* mmap(void* addr, size_t len, bool is_kernel);

size_t get_heap_length();

#endif //NIGHTOS_MEMMGR_H
