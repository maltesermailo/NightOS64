//
// Created by Jannik on 31.03.2024.
//

#ifndef NIGHTOS_GDT_H
#define NIGHTOS_GDT_H

#include <stdint.h>

struct gdt_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
    uint32_t base_top;
} __attribute__((packed));

typedef struct tss_entry {
    uint32_t reserved_0;
    uint64_t rsp[3];
    uint64_t reserved_1;
    uint64_t ist[7];
    uint64_t reserved_2;
    uint16_t reserved_3;
    uint16_t iomap_base;
} __attribute__ ((packed)) tss_entry_t;

struct gdt {
    struct gdt_descriptor gdt[5];
    struct tss_descriptor tss;
} __attribute__((packed));

typedef struct {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed)) gdt_pointer_t;

void gdt_install();
void set_stack_pointer(uintptr_t stack);

#endif //NIGHTOS_GDT_H
