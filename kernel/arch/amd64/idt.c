//
// Created by Jannik on 01.04.2024.
//

#include "../../idt.h"
#include <stdint.h>
#include "io.h"
#include "../../terminal.h"
#include "../../proc/process.h"

typedef struct {
    uint16_t    isr_low;      // The lower 16 bits of the ISR's address
    uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
    uint8_t	    ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
    uint8_t     attributes;   // Type and attributes; see the IDT page
    uint16_t    isr_mid;      // The higher 16 bits of the lower 32 bits of the ISR's address
    uint32_t    isr_high;     // The higher 32 bits of the ISR's address
    uint32_t    reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t	limit;
    uint64_t	base;
} __attribute__((packed)) idtr_t;

__attribute__((aligned(0x10)))
static idt_entry_t idt[256];

static idtr_t idtr;
static irq_handler_t irqHandlers[16];

extern void* isr_stub_table[];
extern void* isr_stub_128;

void exception_handler(regs_t * regs) {
    if(regs->int_no < 32) {
        if(regs->int_no == 1) {
            __asm__ volatile("sti");
            return;
        }

        printf("exception :(\n");
        printf("no: %d\n", regs->int_no);
        printf("err: 0x%x\n", regs->err_code);

        __asm__ volatile ("cli");
        asm volatile("hlt");
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        if(irqHandlers[regs->int_no - 32]) {
            irqHandlers[regs->int_no - 32](regs);
        }
    } else if(regs->int_no == 0x80) {
        //Syscalls baby!
        __asm__ volatile("sti");
        syscall_entry(regs);
    }

    if(get_current_process() != NULL) {
        process_check_signals(regs);
    }

    __asm__ volatile("sti");
}

void sti(uint64_t rflags) {
    // Restore RFLAGS
    __asm__ volatile (
            "pushq %0\n\t"
            "popfq"
            :
            : "r" (rflags)
            );
}

uint64_t cli() {
    uint64_t rflags;

    // Save RFLAGS
    __asm__ volatile (
            "pushfq\n\t"
            "popq %0"
            : "=r" (rflags)
            );

    __asm__ volatile("cli");

    return rflags;
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = 0x08;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}

void irq_install_handler(size_t irq, irq_handler_t handler) {
    if(!irqHandlers[irq]) {
        irqHandlers[irq] = handler;
    }
}

void irq_install() {

}

void idt_install() {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * 256 - 1;

    for (uint8_t vector = 0; vector < 48; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
    }

    //Set Page fault stack
    idt[0xD].ist = 0x1;

    //remember to reroute this if more isrs come, rn syscall is at 48
    idt_set_descriptor(0x80, isr_stub_table[48], 0x8E | 0x60);

    __asm__ volatile ("lidt %0" : : "m"(idtr)); // load the new IDT
}