//
// Created by Jannik on 19.06.2024.
//
#include "../idt.h"
#include "../proc/process.h"
#include "../../libc/include/string.h"
#include <stdio.h>
#include "../memmgr.h"
#include "../../mlibc/abis/linux/errno.h"
#include "../timer.h"

typedef int (*syscall_t)(long,long,long,long,long);

int sys_read(long fd, long buffer, long size) {
    process_t* proc = get_current_process();

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(!CHECK_PTR(buffer)) {
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

    if(!CHECK_PTR(buffer)) {
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
        uintptr_t result = (uintptr_t) mmap((void *) proc->page_directory->heap, size - proc->page_directory->heap, false);
        return result == UINT64_MAX;
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

long sys_mmap(unsigned long address, unsigned long length, long prot, long flags, long fd, long offset) {
    //PROT, FLAGS, FD and OFFSET are ignored currently
    //SPECIFYING A DIFFERENT VALUE THAN 0 FOR FD AND OFFSET WILL RESULT IN AN ERROR
    if(fd != 0 || offset != 0) {
        return -ENOSYS;
    }

    uintptr_t result = (uintptr_t) mmap((void *) address, length, 0);

    return result;
}

long sys_mprotect(unsigned long address, unsigned long size, long prot) {
    return -ENOSYS;
}

long sys_munmap(unsigned long address, unsigned long length)  {
    munmap((void *) address, length);

    return 0;
}

long sys_tcb_set(uintptr_t tcb) {
    if(!CHECK_PTR(tcb)) {
        return -EINVAL;
    }

    uint32_t high = tcb >> 32;
    uint32_t low = tcb & 0xFFFFFFFF;

    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(0xC0000100));

    return 0;
}

long sys_ioctl() {
    return 0;
}

long sys_yield() {
    schedule(false);

    return 0;
}

long sys_execve(long pathname, long argv, long envp) {
    if(CHECK_PTR(pathname) || CHECK_PTR(argv) || CHECK_PTR(envp)) {
        return -EINVAL;
    }

    if((void *) pathname == NULL) {
        return -EINVAL;
    }

    char* path = (char*)pathname;
    char** args = (char**)argv;
    char** env = (char**)envp;

    if(args != NULL) {
        char* arg = NULL;
        int i = 0;

        arg = args[0];

        while(arg != NULL) {
            if(i > 65536) {
                return -EINVAL;
            }

            i++;
            arg = args[i];
        }
    }

    if(env != NULL) {
        char* arg = NULL;
        int i = 0;

        arg = env[0];

        while(arg != NULL) {
            if(i > 65536) {
                return -EINVAL;
            }

            i++;
            arg = env[i];
        }
    }

    return execve(path, args, env);
}

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

long sys_futex(long pointer, long action, long val, long timeout) {
    if(!CHECK_PTR(pointer)) {
        return -EINVAL;
    }

    //check if pointer is mapped
    if(memmgr_create_or_get_page(pointer, 0, 0) == 0) {
        return -EINVAL;
    }

    uint32_t* ptr = (uint32_t*)pointer;

    if(action == FUTEX_WAIT) {
        if(__sync_fetch_and_add(ptr, 0) == val) {
            sleep(10); //We're just gonna sleep for 10 milliseconds then reschedule; User space shall carry first
        } else {
            return -EAGAIN;
        }
    } else if(action == FUTEX_WAKE) {
        return 0;
    }
}

//Stub for unimplemented syscalls to fill
int sys_stub() {
    return -1;
}

syscall_t syscall_table[202] = {
        (syscall_t)sys_read,    //SYS_READ
        (syscall_t)sys_write,   //SYS_WRITE
        (syscall_t)sys_open,    //SYS_OPEN
        (syscall_t)sys_close,   //SYS_CLOSE
        (syscall_t)sys_stub,    //SYS_STAT
        (syscall_t)sys_stub,    //SYS_FSTAT
        (syscall_t)sys_stub,    //SYS_LSTAT
        (syscall_t)sys_stub,    //SYS_POLL
        (syscall_t)sys_lseek,   //SYS_LSEEK
        (syscall_t)sys_mmap,    //SYS_MMAP
        (syscall_t)sys_mprotect,//SYS_MPROTECT
        (syscall_t)sys_munmap,  //SYS_MUNMAP
        (syscall_t)sys_brk,     //SYS_BRK
        (syscall_t)sys_stub,    //SYS_RT_SIGACTION
        (syscall_t)sys_stub,    //SYS_SIGPROCMASK
        (syscall_t)sys_stub,    //SYS_RT_SIGRETURN
        (syscall_t)sys_ioctl,   //SYS_IOCTL
        (syscall_t)sys_stub,    //SYS_PREAD64
        (syscall_t)sys_stub,    //SYS_PWRITE64
        (syscall_t)sys_stub,    //SYS_READV
        (syscall_t)sys_stub,    //SYS_WRITEV
        (syscall_t)sys_stub,    //SYS_ACCESS
        (syscall_t)sys_stub,    //SYS_PIPE
        (syscall_t)sys_stub,    //SYS_SELECT
        (syscall_t)sys_yield,   //SYS_SCHED_YIELD
        (syscall_t)sys_stub,    //SYS_MREMAP
        (syscall_t)sys_stub,    //SYS_MYSYNC
        (syscall_t)sys_stub,    //SYS_MINCORE
        (syscall_t)sys_stub,    //SYS_MADVISE
        (syscall_t)sys_tcb_set, //SYS_TEMP_TCB_SET(will be replaced later)
        (syscall_t)sys_stub,    //SYS_SHMAT
        (syscall_t)sys_stub,    //SYS_SHMCTL
        (syscall_t)sys_stub,    //SYS_DUP
        (syscall_t)sys_stub,    //SYS_DUP2
        (syscall_t)sys_stub,    //SYS_PAUSE
        (syscall_t)sys_stub,    //SYS_NANOSLEEP
        (syscall_t)sys_stub,    //SYS_GETITIMER
        (syscall_t)sys_stub,    //SYS_ALARM
        (syscall_t)sys_stub,    //SYS_SETITIMER
        (syscall_t)sys_getpid,  //SYS_GETPID
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
        (syscall_t)sys_execve, //SYS_EXECVE
        (syscall_t)sys_exit, //SYS_EXIT
        (syscall_t)sys_stub, //SYS_WAIT4
        (syscall_t)sys_stub,  //SYS_KILL
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
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_stub,
        (syscall_t)sys_futex,
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