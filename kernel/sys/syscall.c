//
// Created by Jannik on 19.06.2024.
//
#include "../idt.h"
#include "../proc/process.h"
#include <string.h>

typedef int (*syscall_t)(long,long,long,long,long);

int sys_read(long fd, long buffer, long size) {
    process_t* proc = get_current_process();

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    int readBytes = read(handle, (char*)buffer, size);

    return readBytes;
}

int sys_write(long fd, long buffer, long size) {
    process_t* proc = get_current_process();

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    int written = write(handle, (char*)buffer, size);

    return written;
}

int sys_open(long ptr, long mode) {
    char* nameBuf = (char*)ptr;
    int nameLen = strlen(nameBuf);

    if(nameLen > 4096) {
        return -1; //dafuq?
    }

    file_node_t* node = open(nameBuf, (int)mode);

    if(node == NULL) {
        return -1;
    }

    int fd = process_open_fd(node, mode);

    return fd;
}

syscall_t syscall_table[3] = {
        (syscall_t)sys_read,
        (syscall_t)sys_write,
        (syscall_t)sys_open
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