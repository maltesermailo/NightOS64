//
// Created by Jannik on 01.04.2024.
//
#include "../../gdt.h"
#include <stdint.h>

static gdt_pointer_t pointer;
static tss_entry_t tss;

extern void* stack_top;
extern void reloadSegments();

/**
 * This functions loads the installed GDT from start.S and modifies the TSS to point towards our kernel stack
 * and also make it accessible for context switch
 */
void gdt_install() {
    __asm__("sgdt %0" : "=m"(pointer) : : "memory");

    struct gdt* gdt;
    gdt = (struct gdt *) pointer.base;

    uintptr_t addr = (uintptr_t)&tss;

    gdt->tss.limit_low = sizeof(tss);
    gdt->tss.base_low = (addr & 0xFFFF);
    gdt->tss.base_middle = (addr >> 16) & 0xFF;
    gdt->tss.base_high = (addr >> 24) & 0xFF;
    gdt->tss.base_top = (addr >> 32) & 0xFFFFFFFF;

    tss.rsp[0] = (uintptr_t)stack_top;
    tss.iomap_base = sizeof(tss);

    asm volatile("lgdt %0" : : "m"(pointer));
    asm volatile("ltr %%ax" : : "a" (0x28));

    reloadSegments();
}

void set_stack_pointer(uintptr_t stack) {
    tss.rsp[0] = stack;
}

void set_ist(int index, uintptr_t stack) {
    tss.ist[index] = stack;
}