//
// Created by Jannik on 01.04.2024.
//

#ifndef NIGHTOS_IDT_H
#define NIGHTOS_IDT_H

#include <stdint.h>
#include <stddef.h>

typedef struct exception_registers {
    uintptr_t r15, r14, r13, r12, r11, r10, r9, r8;
    uintptr_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    uintptr_t int_no, err_code;

    uintptr_t rip, cs, rflags, rsp, ss;
} regs_t;

typedef void (*irq_handler_t)(regs_t* regs);

void irq_install_handler(size_t irq, irq_handler_t handler);

void pic_disable();
void pic_setup();
void pic_sendEOI(uint8_t irq);
void idt_install();
void irq_install();

void exception_handler(regs_t * regs);

#endif //NIGHTOS_IDT_H
