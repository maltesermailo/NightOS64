//
// Created by Jannik on 19.06.2024.
//
#include "../idt.h"
typedef int (*syscall_t)(long,long,long,long,long);

int sys_read(long fd, long buffer, long size) {
    return 0;
}

syscall_t syscall_table[2] = {
        (syscall_t)sys_read
};

void syscall_entry(regs_t* regs) {
    uintptr_t syscallNo = regs->rax;

    if(syscall_table[syscallNo]) {
        int returnCode = syscall_table[syscallNo](regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);

        regs->rax = returnCode;
        return;
    }

    regs->rax = -1;
}

void syscall_init() {

}