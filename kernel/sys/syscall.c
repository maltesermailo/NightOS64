//
// Created by Jannik on 19.06.2024.
//
#include "../idt.h"
#include "../proc/process.h"
#include <string.h>
#include <stdio.h>
#include "../memmgr.h"

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

int sys_close(long fd) {
    process_close_fd(fd);

    return 0;
}

int sys_lseek(long fd, long offset, long whence) {
    process_t* proc = get_current_process();

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    switch(whence) {
        case SEEK_SET:
            handle->offset = offset;
            break;
        case SEEK_CUR:
            handle->offset += offset;
            break;
        case SEEK_END:
            handle->offset = handle->fileNode->size + offset;
            break;
    }

    return handle->offset;
}

long sys_brk(long increment, long size) {
    process_t* proc = get_current_process();

    if(proc == NULL) return -1;

    spin_lock(&proc->page_directory->lock);

    if(increment == 1) {
        return (uintptr_t) mmap((void *) proc->page_directory->heap, size, false);
    }

    if(size < proc->page_directory->heap) {
        munmap((void *) size, proc->page_directory->heap - size);

        return size;
    } else {
        return (uintptr_t) mmap(proc->page_directory->heap, size - proc->page_directory->heap, false);
    }
}

int sys_exit(long exitCode) {
    return -1;
}

int sys_getpid() {
    if(get_current_process()->tgid != 0) {
        return get_current_process()->tgid;
    }

    return get_current_process()->id;
}

pid_t sys_fork() {
    pid_t pid = process_fork();

    return pid;
}

//Stub for unimplemented syscalls to fill
int sys_stub() {
    return -1;
}

syscall_t syscall_table[63] = {
        (syscall_t)sys_read,
        (syscall_t)sys_write,
        (syscall_t)sys_open,
        (syscall_t)sys_close,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_lseek,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_brk,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_getpid,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_fork,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_exit,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub
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