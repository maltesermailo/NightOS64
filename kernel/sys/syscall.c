//
// Created by Jannik on 19.06.2024.
//
#include "../idt.h"
#include "../proc/process.h"
#include "../../libc/include/string.h"
#include "../../libc/include/kernel/hashtable.h"
#include <stdio.h>
#include "../memmgr.h"
#include "../../mlibc/abis/linux/errno.h"
#include "../timer.h"
#include <bits/ansi/time_t.h>
#include <signal.h>
#include <abi-bits/stat.h>
#include <abi-bits/access.h>
#include <bits/posix/iovec.h>
#include "../../mlibc/abis/linux/fcntl.h"
#include "../../mlibc/abis/linux/poll.h"
#include "../../mlibc/options/posix/include/sys/poll.h"
#include "../terminal.h"

typedef int (*syscall_t)(long,long,long,long,long);

spin_t* futex_lock;
struct hashtable* futex_queue;

int sys_read(long fd, long buffer, long size) {
    process_t* proc = get_current_process();

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    if(handle == NULL) {
        return -1;
    }

    if(!CHECK_PTR(buffer)) {
        return -1;
    }

    int readBytes = read(handle, (char*)buffer, size);
    handle->offset += readBytes;

    return readBytes;
}

int sys_write(long fd, long buffer, long size) {
    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(!CHECK_PTR(buffer)) {
        return -1;
    }

    int written = write(handle, (char*)buffer, size);
    handle->offset += written;

    return written;
}

int sys_open(long ptr, long mode) {
    char* nameBuf = (char*)ptr;
    int nameLen = strlen(nameBuf);

    if(nameLen > 4096) {
        return -EFAULT; //dafuq?
    }

    file_node_t* node = open(nameBuf, (int)mode);

    if(node == NULL) {
        return -ENOENT;
    }

    int fd = process_open_fd(node, mode);

    return fd;
}

int sys_close(long fd) {
    process_close_fd(fd);

    return 0;
}

long sys_stat(long path, long statbuf) {
    if(!CHECK_PTR(path) || !CHECK_PTR(statbuf)) {
        return -EINVAL;
    }

    file_node_t* node = open((char*)path, 0);

    if(node == NULL) {
        return -ENOENT;
    }

    struct stat* stat_struct = (struct stat*)statbuf;
    stat_struct->st_dev = 0;
    stat_struct->st_ino = 0;
    stat_struct->st_nlink = 0;

    switch(node->type) {
        case FILE_TYPE_MOUNT_POINT:
        case FILE_TYPE_DIR:
            stat_struct->st_mode = S_IFDIR;
            break;
        case FILE_TYPE_FILE:
            stat_struct->st_mode = S_IFREG;
            break;
        case FILE_TYPE_VIRTUAL_DEVICE:
            stat_struct->st_mode = S_IFCHR;
            break;
        case FILE_TYPE_BLOCK_DEVICE:
            stat_struct->st_mode = S_IFBLK;
            break;
        case FILE_TYPE_NAMED_PIPE:
            stat_struct->st_mode = S_IFIFO;
            break;
        case FILE_TYPE_SOCKET:
            stat_struct->st_mode = S_IFSOCK;
            break;
        default:
            stat_struct->st_mode = S_IFREG;
    }

    stat_struct->st_mode |= (S_IRWXU | S_IRWXG);
    stat_struct->st_uid = 0;
    stat_struct->st_gid = 0;
    stat_struct->st_blksize = 512;
    stat_struct->st_size = node->size;
    stat_struct->st_blocks = node->size / 512;
    stat_struct->st_atim.tv_sec = 0;
    stat_struct->st_atim.tv_nsec = 0;

    stat_struct->st_mtim.tv_sec = 0;
    stat_struct->st_mtim.tv_nsec = 0;

    stat_struct->st_ctim.tv_sec = 0;
    stat_struct->st_ctim.tv_nsec = 0;

    return 0;
}

long sys_lstat(long path, long statbuf) {
    if(!CHECK_PTR(path) || !CHECK_PTR(statbuf)) {
        return -EINVAL;
    }

    file_node_t* node = open((char*)path, 0);

    if(node == NULL) {
        return -ENOENT;
    }

    struct stat* stat_struct = (struct stat*)statbuf;
    stat_struct->st_dev = 0;
    stat_struct->st_ino = 0;
    stat_struct->st_nlink = 0;

    switch(node->type) {
        case FILE_TYPE_MOUNT_POINT:
        case FILE_TYPE_DIR:
            stat_struct->st_mode = S_IFDIR;
            break;
        case FILE_TYPE_FILE:
            stat_struct->st_mode = S_IFREG;
            break;
        case FILE_TYPE_VIRTUAL_DEVICE:
            stat_struct->st_mode = S_IFCHR;
            break;
        case FILE_TYPE_BLOCK_DEVICE:
            stat_struct->st_mode = S_IFBLK;
            break;
        case FILE_TYPE_NAMED_PIPE:
            stat_struct->st_mode = S_IFIFO;
            break;
        case FILE_TYPE_SOCKET:
            stat_struct->st_mode = S_IFSOCK;
            break;
        default:
            stat_struct->st_mode = S_IFREG;
    }

    stat_struct->st_mode |= (S_IRWXU | S_IRWXG);
    stat_struct->st_uid = 0;
    stat_struct->st_gid = 0;
    stat_struct->st_blksize = 512;
    stat_struct->st_size = node->size;
    stat_struct->st_blocks = node->size / 512;
    stat_struct->st_atim.tv_sec = 0;
    stat_struct->st_atim.tv_nsec = 0;

    stat_struct->st_mtim.tv_sec = 0;
    stat_struct->st_mtim.tv_nsec = 0;

    stat_struct->st_ctim.tv_sec = 0;
    stat_struct->st_ctim.tv_nsec = 0;

    return 0;
}

long sys_fstat(long fd, long statbuf) {
    if(!CHECK_PTR(statbuf)) {
        return -EINVAL;
    }

    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -EINVAL;
    }

    struct stat* stat_struct = (struct stat*)statbuf;
    stat_struct->st_dev = 0;
    stat_struct->st_ino = 0;
    stat_struct->st_nlink = 0;

    switch(handle->fileNode->type) {
        case FILE_TYPE_MOUNT_POINT:
        case FILE_TYPE_DIR:
            stat_struct->st_mode = S_IFDIR;
            break;
        case FILE_TYPE_FILE:
            stat_struct->st_mode = S_IFREG;
            break;
        case FILE_TYPE_VIRTUAL_DEVICE:
            stat_struct->st_mode = S_IFCHR;
            break;
        case FILE_TYPE_BLOCK_DEVICE:
            stat_struct->st_mode = S_IFBLK;
            break;
        case FILE_TYPE_NAMED_PIPE:
            stat_struct->st_mode = S_IFIFO;
            break;
        case FILE_TYPE_SOCKET:
            stat_struct->st_mode = S_IFSOCK;
            break;
        default:
            stat_struct->st_mode = S_IFREG;
    }

    stat_struct->st_mode |= (S_IRWXU | S_IRWXG);
    stat_struct->st_uid = 0;
    stat_struct->st_gid = 0;
    stat_struct->st_blksize = 512;
    stat_struct->st_size = handle->fileNode->size;
    stat_struct->st_blocks = handle->fileNode->size / 512;
    stat_struct->st_atim.tv_sec = 0;
    stat_struct->st_atim.tv_nsec = 0;

    stat_struct->st_mtim.tv_sec = 0;
    stat_struct->st_mtim.tv_nsec = 0;

    stat_struct->st_ctim.tv_sec = 0;
    stat_struct->st_ctim.tv_nsec = 0;

    return 0;
}

int sys_poll(long fdbuf, long nfds, int timeout) {
    struct pollfd* pollfds = (struct pollfd*) fdbuf;

    for(int i = 0; i < nfds; i++) {
        if(!CHECK_PTR((uintptr_t) pollfds)) {
            return -EFAULT;
        }

        pollfds++;
    }

    bool one_ready = false;
    uint64_t time_at_start = get_counter();
    uint64_t time_end = time_at_start + (timeout / 10) + 1;

    while(!one_ready) {
        pollfds = (struct pollfd*) fdbuf;

        for(int i = 0; i < nfds; i++) {
            if(pollfds->fd > get_current_process()->fd_table->capacity) {
                pollfds->revents = POLLNVAL;

                pollfds++;
                continue;
            }

            file_handle_t* handle = get_current_process()->fd_table->handles[pollfds->fd];

            if(handle == NULL) {
                pollfds->revents = POLLNVAL;

                pollfds++;
                continue;
            }

            pollfds->revents = (short)poll(handle, pollfds->events);

            if((pollfds->revents & (POLLIN | POLLOUT))) {
                one_ready = true;
            }
        }

        if(!one_ready) {
            if(time_end < get_counter()) {
                return 0;
            }

            schedule(false);
        }
    }

    int countChanged = 0;

    for(int i = 0; i < nfds; i++) {
        if(pollfds->revents & (POLLIN | POLLOUT)) {
            countChanged++;
        }
    }

    return countChanged;
}

int sys_lseek(long fd, long offset, long whence) {
    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(handle->fileNode->type == FILE_TYPE_VIRTUAL_DEVICE || handle->fileNode->type == FILE_TYPE_NAMED_PIPE) {
        return -ESPIPE;
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

int sys_pread64(long fd, unsigned long buffer, unsigned long size, unsigned long offset) {
    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(!CHECK_PTR(buffer)) {
        return -1;
    }

    //Use temporary handle to set offset without impacting other threads
    file_handle_t tempHandle = {
            .offset = offset,
            .fileNode = handle->fileNode,
            .mode = handle->mode
    };

    int readBytes = read(&tempHandle, (char*)buffer, size);

    return readBytes;
}

int sys_pwrite64(long fd, unsigned long buffer, unsigned long size, unsigned long offset) {
    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(!CHECK_PTR(buffer)) {
        return -1;
    }

    //Use temporary handle to set offset without impacting other threads
    file_handle_t tempHandle = {
            .offset = offset,
            .fileNode = handle->fileNode,
            .mode = handle->mode
    };

    int written = write(&tempHandle, (char*)buffer, size);

    return written;
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

long sys_rt_sigaction(long signum, long newact, long oldact) {
    process_t* proc = get_current_process();

    if(proc == NULL) return -1;

    if(!CHECK_PTR(newact) || !CHECK_PTR(oldact) || signum > 32) {
        return -EINVAL;
    }

    struct sigaction* signew = (struct sigaction*)newact;

    if(oldact != NULL) {
        struct sigaction* sigold = (struct sigaction*) oldact;
        sigold->sa_handler = process_get_signal_handler(signum)->handler;
        sigold->sa_mask = process_get_signal_handler(signum)->sa_mask;
        sigold->sa_flags = process_get_signal_handler(signum)->sa_flags;
    }

    if(signew->sa_handler) {
        process_set_signal_handler(signum, signew);
    }

    return 0;
}

long sys_rt_sigprocmask(long how, long newsigset, long oldsigset) {
    process_t* proc = get_current_process();

    if(proc == NULL) return -1;

    if(!CHECK_PTR(newsigset) || !CHECK_PTR(newsigset)) {
        return -EINVAL;
    }

    if(oldsigset != NULL) {
        sigset_t* sigold = (sigset_t*) oldsigset;

        for (int i = 0; i < SIGSET_NWORDS; i++) {
            sigold->sig[i] = proc->blocked_signals.sig[i];
        }
    }

    sigset_t* sigset = (sigset_t*)newsigset;
    process_set_signal_mask(how, sigset);

    return 0;
}

long sys_rt_sigreturn() {
    return process_signal_return();
}

long sys_readv(long fd, const struct iovec* iov, int iovcnt) {
    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(!CHECK_PTR((uintptr_t) iov)) {
        return -EINVAL;
    }

    int totalBytesRead = 0;

    for(int i = 0; i < iovcnt; i++) {
        int readBytes = read(handle, (char*)(iov->iov_base), iov->iov_len);
        handle->offset += readBytes;
        totalBytesRead += readBytes;

        iov++;
    }

    return totalBytesRead;
}

long sys_writev(long fd, const struct iovec* iov, int iovcnt) {
    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(!CHECK_PTR((uintptr_t) iov)) {
        return -EINVAL;
    }

    int totalBytesWritten = 0;

    for(int i = 0; i < iovcnt; i++) {
        int written = write(handle, (char*)(iov->iov_base), iov->iov_len);
        handle->offset += written;
        totalBytesWritten += written;

        iov++;
    }

    return totalBytesWritten;
}

long sys_access(long pathPtr, long mode) {
    //TODO: Add support for symbolic links
    if(!CHECK_PTR(pathPtr)) {
        return -EFAULT;
    }

    char* path = (char*)pathPtr;

    file_node_t* node = open(path, 0);

    if(node == NULL) {
        return -ENOENT;
    }

    process_t* process = get_current_process();

    if(process->uid == 0 || process->gid == 0) {
        //Superuser always has access
        return 0;
    }

    if(mode == F_OK) {
        return 0;
    }

    if(node->owner == process->uid) {
        //Now check the access mask
        if(mode & R_OK && (node->access_mask & S_IRUSR) == 0) {
            return -EACCES;
        }

        if(mode & W_OK && (node->access_mask & S_IWUSR) == 0) {
            return -EACCES;
        }

        if(mode & X_OK && (node->access_mask & S_IXUSR) == 0) {
            return -EACCES;
        }

        return 0;
    }

    if(node->owner_group == process->gid) {
        //Now check the access mask
        if(mode & R_OK && (node->access_mask & S_IRGRP) == 0) {
            return -EACCES;
        }

        if(mode & W_OK && (node->access_mask & S_IWGRP) == 0) {
            return -EACCES;
        }

        if(mode & X_OK && (node->access_mask & S_IXGRP) == 0) {
            return -EACCES;
        }

        return 0;
    }

    //Now check the access mask
    if(mode & R_OK && (node->access_mask & S_IROTH) == 0) {
        return -EACCES;
    }

    if(mode & W_OK && (node->access_mask & S_IWOTH) == 0) {
        return -EACCES;
    }

    if(mode & X_OK && (node->access_mask & S_IXOTH) == 0) {
        return -EACCES;
    }

    return 0;
}

[[noreturn]] int sys_exit(long exitCode) {
    process_thread_exit(exitCode);
}

int sys_getcwd(long buf, long size) {
    if(!CHECK_PTR(buf)) {
        return -EFAULT;
    }

    if(size == 0 || buf == NULL) {
        return -EINVAL;
    }

    char* cwd = get_cwd_name();

    if(strlen(cwd) > size) {
        return -ERANGE;
    }

    strcpy((char*)buf, cwd);

    return 0;
}

int sys_getpid() {
    if(get_current_process()->tgid != 0) {
        return get_current_process()->tgid;
    }

    return get_current_process()->id;
}

int sys_getpgrp() {
    if(get_current_process()->tgid != 0) {
        return get_current_process()->tgid;
    }

    return get_current_process()->id;
}

int sys_getppid() {
    return get_current_process()->parent;
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
        return -EFAULT;
    }

    uint32_t high = tcb >> 32;
    uint32_t low = tcb & 0xFFFFFFFF;

    get_current_process()->main_thread.tls_base = tcb;

    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(0xC0000100));

    return 0;
}

long sys_dup(long fd) {
    process_t* proc = get_current_process();

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    int newfd = process_open_fd(handle->fileNode, handle->flags);

    file_handle_t* newHandle = proc->fd_table->handles[newfd];
    newHandle->offset = handle->offset;
    newHandle->flags = handle->flags;

    return newfd;
}

long sys_nanosleep(struct timespec* timespec) {
    if(!CHECK_PTR((uintptr_t) timespec)) {
        return -EFAULT;
    }

    long nanosecondsToMs = timespec->tv_nsec / 1000000;

    sleep(timespec->tv_sec * 1000 + nanosecondsToMs + 1);
}

long sys_ioctl(long fd, unsigned long operation, unsigned long args) {
    process_t* proc = get_current_process();

    if(!CHECK_PTR(args)) {
        return -EFAULT;
    }

    if(fd > proc->fd_table->capacity) {
        return -1;
    }

    file_handle_t* handle = proc->fd_table->handles[fd];

    if(handle == NULL) {
        return -1;
    }

    if(handle->fileNode->file_ops.ioctl) {
        return handle->fileNode->file_ops.ioctl(handle->fileNode, operation, (void*)args);
    }

    return -ENOTTY;
}

long sys_yield() {
    schedule(false);

    return 0;
}

long sys_clone(unsigned long flags, unsigned long stack, unsigned long parent_tid, unsigned long child_tid, unsigned long tls) {
    if(!(CHECK_PTR(flags) && CHECK_PTR(stack) && CHECK_PTR(parent_tid) && CHECK_PTR(child_tid) && CHECK_PTR(tls))) {
        return -EFAULT;
    }

    struct clone_args args;
    args.flags = flags;
    args.stack = stack;
    args.parent_tid = parent_tid;
    args.child_tid = child_tid;
    args.tls = tls;

    return process_clone(&args, sizeof(struct clone_args));
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

long sys_rename(long oldpathptr, long newpathptr) {
    if(!CHECK_PTR(oldpathptr) || !CHECK_PTR(newpathptr)) {
        return -EFAULT;
    }

    char* old_path = (char*) oldpathptr;

    file_node_t* old = open(old_path, 0);

    if(old == NULL) {
        return -ENOENT;
    }

    return move_file(old, (char*)newpathptr, 0);
}

long sys_mkdir(long pathptr) {
    if(!CHECK_PTR(pathptr)) {
        return -EFAULT;
    }

    char* path = (char*) pathptr;

    file_node_t* node = open(path, 0);

    if(node != NULL) {
        return -EEXIST;
    }

    node = mkdir(path);

    if(node == NULL) {
        return -ENOSPC;
    }

    return 0;
}

long sys_rmdir(long pathptr) {
    if(!CHECK_PTR(pathptr)) {
        return -EFAULT;
    }

    char* path = (char*) pathptr;

    file_node_t* node = open(path, 0);

    if(node == NULL) {
        return -ENOENT;
    }

    if(node->type != FILE_TYPE_DIR) {
        return -ENOTDIR;
    }

    return delete(path);
}

long sys_create(long pathptr) {
    return sys_open(pathptr, O_CREAT | O_WRONLY | O_TRUNC);
}

long sys_link(long fd, long pathptr) {
    if(!CHECK_PTR(pathptr)) {
        return -EFAULT;
    }

    char* path = (char*) pathptr;

    file_node_t* node = open(path, 0);

    if(node != NULL) {
        return -EEXIST;
    }

    if(fd > get_current_process()->fd_table->capacity) {
        return -EFAULT;
    }

    file_handle_t* handle = get_current_process()->fd_table->handles[fd];

    if(!handle) {
        return -EFAULT;
    }

    return link(handle, path);
}

long sys_unlink(long pathptr) {
    if(!CHECK_PTR(pathptr)) {
        return -EFAULT;
    }

    char* path = (char*) pathptr;

    file_node_t* node = open(path, 0);

    if(node == NULL) {
        return -ENOENT;
    }

    return delete(path);
}

long sys_getuid() {
    process_t* current = get_current_process();

    return current->uid;
}

long sys_getgid() {
    process_t* current = get_current_process();

    return current->gid;
}

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

char* long_to_string(long id) {
    // Maximum number of digits in a long (including sign and null terminator)
    int max_digits = snprintf(NULL, 0, "%ld", id) + 1;
    char* str = malloc(max_digits);
    if (str == NULL) {
        // Handle memory allocation failure
        return NULL;
    }
    snprintf(str, max_digits, "%ld", id);
    return str;
}

long sys_futex(long pointer, long action, long val, long timeout) {
    if(!CHECK_PTR(pointer) || !CHECK_PTR(timeout)) {
        return -EINVAL;
    }

    //check if pointer is mapped
    if(memmgr_create_or_get_page(pointer, 0, 0) == 0) {
        return -EINVAL;
    }

    uint32_t* ptr = (uint32_t*)pointer;
    struct timespec* timespec = (struct timespec*)timeout;

    if(action == FUTEX_WAIT) {
        if(__sync_fetch_and_add(ptr, 0) == val) {
            long nanosecondsToMs = timespec->tv_nsec / 1000000;

            char* id = long_to_string(pointer);

            spin_lock(futex_lock);
            ht_insert(futex_queue, id, get_current_process());
            spin_unlock(futex_lock);

            sleep(timespec->tv_sec * 1000 + nanosecondsToMs + 1); //We're just gonna sleep for 10 milliseconds then reschedule; User space shall carry first

            spin_lock(futex_lock);
            ht_remove_by_key_and_value(futex_queue, id, get_current_process());
            spin_unlock(futex_lock);

            free(id);
        } else {
            return -EAGAIN;
        }
    } else if(action == FUTEX_WAKE) {
        char* id = long_to_string(pointer);

        value_list* list = ht_get_all_values(futex_queue, id);
        if (list != NULL) {
            for (int i = 0; i < list->count; i++) {
                wakeup_now((process_t*)list->values[i]);
            }
            free_value_list(list);
        }

        free(id);

        return -EAGAIN;
    }

    return -ENOSYS;
}

[[noreturn]] int sys_exit_group(long exitCode) {
    process_exit(exitCode);
}

int sys_ppoll(long fdbuf, long nfds, int timeout, long sigmask) {
    if(!CHECK_PTR(sigmask)) {
        return -EFAULT;
    }

    sigset_t old_sigset;
    sigset_t* sigset = (sigset_t*)sigmask;

    sys_rt_sigprocmask(SIG_SETMASK, sigset, &old_sigset);
    int result = sys_poll(fdbuf, nfds, timeout);
    sys_rt_sigprocmask(SIG_SETMASK, &old_sigset, NULL);

    return result;
}

//Stub for unimplemented syscalls to fill
int sys_stub() {
    return -ENOSYS;
}
/**
 * We're gonna implement a custom syscall specifically for the microkernel portion of this operating system
 * SYS_SRVCTL
 *
 * int srvctl(long operation, int flags, void* message);
 *
 * Operations can be one of
 *
 * SRV_SEND: Send a message to a server
 * SRV_CALL: Send a message with response
 * SRV_RECEIVE: Receive a message from a queue
 * SRV_REGISTER: Register a server
 * SRV_UNGREGISTER: Delete a server
 */

syscall_t syscall_table[272] = {
        [0] = (syscall_t)sys_read,    //SYS_READ
        [1] = (syscall_t)sys_write,   //SYS_WRITE
        [2] = (syscall_t)sys_open,    //SYS_OPEN
        [3] = (syscall_t)sys_close,   //SYS_CLOSE
        [4] = (syscall_t)sys_stat,    //SYS_STAT
        [5] = (syscall_t)sys_fstat,   //SYS_FSTAT
        [6] = (syscall_t)sys_lstat,   //SYS_LSTAT
        [7] = (syscall_t)sys_poll,    //SYS_POLL
        [8] = (syscall_t)sys_lseek,   //SYS_LSEEK
        [9] = (syscall_t)sys_mmap,    //SYS_MMAP
        [10] = (syscall_t)sys_mprotect,//SYS_MPROTECT
        [11] = (syscall_t)sys_munmap,  //SYS_MUNMAP
        [12] = (syscall_t)sys_brk,     //SYS_BRK
        [13] = (syscall_t)sys_rt_sigaction,//SYS_RT_SIGACTION
        [14] = (syscall_t)sys_rt_sigprocmask,//SYS_SIGPROCMASK
        [15] = (syscall_t)sys_rt_sigreturn,//SYS_RT_SIGRETURN
        [16] = (syscall_t)sys_ioctl,   //SYS_IOCTL
        [17] = (syscall_t)sys_pread64, //SYS_PREAD64
        [18] = (syscall_t)sys_pwrite64,//SYS_PWRITE64
        [19] = (syscall_t)sys_readv,   //SYS_READV
        [20] = (syscall_t)sys_writev,  //SYS_WRITEV
        [21] = (syscall_t)sys_access,  //SYS_ACCESS
        [22] = (syscall_t)sys_stub,    //SYS_PIPE
        [23] = (syscall_t)sys_stub,    //SYS_SELECT
        [24] = (syscall_t)sys_yield,   //SYS_SCHED_YIELD
        [25] = (syscall_t)sys_stub,    //SYS_MREMAP
        [26] = (syscall_t)sys_stub,    //SYS_MYSYNC
        [27] = (syscall_t)sys_stub,    //SYS_MINCORE
        [28] = (syscall_t)sys_stub,    //SYS_MADVISE
        [29] = (syscall_t)sys_tcb_set, //SYS_TEMP_TCB_SET(will be replaced later)
        [30] = (syscall_t)sys_stub,    //SYS_SHMAT
        [31] = (syscall_t)sys_stub,    //SYS_SHMCTL
        [32] = (syscall_t)sys_dup,    //SYS_DUP
        [33] = (syscall_t)sys_stub,    //SYS_DUP2
        [34] = (syscall_t)sys_stub,    //SYS_PAUSE
        [35] = (syscall_t)sys_nanosleep,//SYS_NANOSLEEP
        [36] = (syscall_t)sys_stub,    //SYS_GETITIMER
        [37] = (syscall_t)sys_stub,    //SYS_ALARM
        [38] = (syscall_t)sys_stub,    //SYS_SETITIMER
        [39] = (syscall_t)sys_getpid,  //SYS_GETPID
        [40] = (syscall_t)sys_stub,    //SYS_SENDFILE
        [41] = (syscall_t)sys_stub,    //SYS_SOCKET
        [42] = (syscall_t)sys_stub,    //SYS_CONNECT
        [43] = (syscall_t)sys_stub,    //SYS_ACCEPT
        [44] = (syscall_t)sys_stub,    //SYS_SENDTO
        [45] = (syscall_t)sys_stub,    //SYS_RECVFROM
        [46] = (syscall_t)sys_stub,    //SYS_SENDMSG
        [47] = (syscall_t)sys_stub,    //SYS_RECVMSG
        [48] = (syscall_t)sys_stub,    //SYS_SHUTDOWN
        [49] = (syscall_t)sys_stub,    //SYS_BIND
        [50] = (syscall_t)sys_stub,    //SYS_LISTEN
        [51] = (syscall_t)sys_stub,    //SYS_GETSOCKNAME
        [52] = (syscall_t)sys_stub,    //SYS_GETPEERNAME
        [53] = (syscall_t)sys_stub,    //SYS_SOCKETPAIR
        [54] = (syscall_t)sys_stub,    //SYS_SETSOCKOPT
        [55] = (syscall_t)sys_stub,    //SYS_GETSOCKOPT
        [56] = (syscall_t)sys_clone,   //SYS_CLONE
        [57] = (syscall_t)sys_fork,    //SYS_FORK
        [58] = (syscall_t)sys_stub,    //SYS_VFORK
        [59] = (syscall_t)sys_execve,  //SYS_EXECVE
        [60] = (syscall_t)sys_exit,    //SYS_EXIT
        [61] = (syscall_t)sys_stub,    //SYS_WAIT4
        [62] = (syscall_t)sys_stub,    //SYS_KILL
        [63] = (syscall_t)sys_stub,    //SYS_UNAME
        [64] = (syscall_t)sys_stub,    //SYS_SEMGET
        [65] = (syscall_t)sys_stub,    //SYS_SEMOP
        [66] = (syscall_t)sys_stub,    //SYS_SEMCTL
        [67] = (syscall_t)sys_stub,    //SYS_SHMDT
        [68] = (syscall_t)sys_stub,    //SYS_MSGGET
        [69] = (syscall_t)sys_stub,    //SYS_MSGSND
        [70] = (syscall_t)sys_stub,    //SYS_MSGRCV
        [71] = (syscall_t)sys_stub,    //SYS_MSGCTL
        [72] = (syscall_t)sys_stub,    //SYS_FCNTL
        [73] = (syscall_t)sys_stub,    //SYS_FLOCK
        [74] = (syscall_t)sys_stub,    //SYS_FSYNC
        [75] = (syscall_t)sys_stub,    //SYS_FDATASYNC
        [76] = (syscall_t)sys_stub,    //SYS_TRUNCATE
        [77] = (syscall_t)sys_stub,    //SYS_FTRUNCATE
        [78] = (syscall_t)sys_stub,    //SYS_GETDENTS
        [79] = (syscall_t)sys_getcwd,    //SYS_GETCWD
        [80] = (syscall_t)sys_stub,    //SYS_CHDIR
        [81] = (syscall_t)sys_stub,    //SYS_FCHDIR
        [82] = (syscall_t)sys_rename,  //SYS_RENAME
        [83] = (syscall_t)sys_mkdir,   //SYS_MKDIR
        [84] = (syscall_t)sys_rmdir,   //SYS_RMDIR
        [85] = (syscall_t)sys_create,  //SYS_CREAT
        [86] = (syscall_t)sys_link,    //SYS_LINK
        [87] = (syscall_t)sys_unlink,  //SYS_UNLINK
        [88] = (syscall_t)sys_stub,    //SYS_SYMLINK
        [89] = (syscall_t)sys_stub,    //SYS_READLINK
        [90] = (syscall_t)sys_stub,    //SYS_CHMOD
        [91] = (syscall_t)sys_stub,    //SYS_FCHMOD
        [92] = (syscall_t)sys_stub,    //SYS_CHOWN
        [93] = (syscall_t)sys_stub,    //SYS_FCHOWN
        [94] = (syscall_t)sys_stub,    //SYS_LCHOWN
        [95] = (syscall_t)sys_stub,    //SYS_UMASK
        [96] = (syscall_t)sys_stub,    //SYS_GETTIMEOFDAY
        [97] = (syscall_t)sys_stub,    //SYS_GETRLIMIT
        [98] = (syscall_t)sys_stub,    //SYS_GETRUSAGE
        [99] = (syscall_t)sys_stub,    //SYS_SYSINFO
        [100] = (syscall_t)sys_stub,    //SYS_TIMES
        [101] = (syscall_t)sys_stub,   //SYS_PTRACE
        [102] = (syscall_t)sys_getuid, //SYS_GETUID
        [103] = (syscall_t)sys_stub,   //SYS_SYSLOG
        [104] = (syscall_t)sys_getgid, //SYS_GETGID
        [105] = (syscall_t)sys_stub,   //SYS_SETUID
        [106] = (syscall_t)sys_stub,   //SYS_SETGID
        [107] = (syscall_t)sys_stub,   //SYS_GETEUID
        [108] = (syscall_t)sys_stub,   //SYS_GETEGID
        [109] = (syscall_t)sys_getpgrp,//SYS_SETPGID
        [110] = (syscall_t)sys_getppid,//SYS_GETPPID
        [111] = (syscall_t)sys_getpgrp,//SYS_GETPGRP
        [112] = (syscall_t)sys_stub,   //SYS_SETSID
        [113] = (syscall_t)sys_stub,   //SYS_SETREUID
        [114] = (syscall_t)sys_stub,   //SYS_SETREGID
        [115] = (syscall_t)sys_stub,   //SYS_GETGROUPS
        [116] = (syscall_t)sys_stub,   //SYS_SETGROUPS
        [117] = (syscall_t)sys_stub,   //SYS_SETRESUID
        [118] = (syscall_t)sys_stub,   //SYS_GETRESUID
        [119] = (syscall_t)sys_stub,   //SYS_SETRESGID
        [120] = (syscall_t)sys_stub,   //SYS_GETRESGID
        [121] = (syscall_t)sys_stub,   //SYS_GETPGID
        [122] = (syscall_t)sys_stub,   //SYS_SETFSUID
        [123] = (syscall_t)sys_stub,   //SYS_SETFSGID
        [124] = (syscall_t)sys_stub,   //SYS_GETSID
        [125] = (syscall_t)sys_stub,   //SYS_CAPGET
        [126] = (syscall_t)sys_stub,   //SYS_CAPSET
        [127] = (syscall_t)sys_stub,   //SYS_RT_SIGPENDING
        [128] = (syscall_t)sys_stub,   //SYS_RT_SIGTIMEDWAIT
        [129] = (syscall_t)sys_stub,   //SYS_RT_SIGQUEUEINFO
        [130] = (syscall_t)sys_stub,   //SYS_RT_SIGSUSPEND
        [131] = (syscall_t)sys_stub,   //SYS_SIGALTSTACK
        [132] = (syscall_t)sys_stub,   //SYS_UTIME
        [133] = (syscall_t)sys_stub,   //SYS_MKNOD
        [134] = (syscall_t)sys_stub,   //SYS_USELIB
        [135] = (syscall_t)sys_stub,   //SYS_PERSONALITY
        [136] = (syscall_t)sys_stub,   //SYS_USTAT
        [137] = (syscall_t)sys_stub,   //SYS_STATFS
        [138] = (syscall_t)sys_stub,   //SYS_FSTATFS
        [139] = (syscall_t)sys_stub,   //SYS_SYSFS
        [140] = (syscall_t)sys_stub,   //SYS_GETPRIORITY
        [141] = (syscall_t)sys_stub,   //SYS_SETPRIORITY
        [142] = (syscall_t)sys_stub,   //SYS_SCHED_SETPARAM
        [143] = (syscall_t)sys_stub,   //SYS_SCHED_GETPARAM
        [144] = (syscall_t)sys_stub,   //SYS_SCHED_SETSCHEDULER
        [145] = (syscall_t)sys_stub,   //SYS_SCHED_GETSCHEDULER
        [146] = (syscall_t)sys_stub,   //SYS_SCHED_GET_PRIORITY_MAX
        [147] = (syscall_t)sys_stub,   //SYS_SCHED_GET_PRIORITY_MIN
        [148] = (syscall_t)sys_stub,   //SYS_SCHED_RR_GET_INTERVAL
        [149] = (syscall_t)sys_stub,   //SYS_MLOCK
        [150] = (syscall_t)sys_stub,   //SYS_MUNLOCK
        [151] = (syscall_t)sys_stub,   //SYS_MLOCKALL
        [152] = (syscall_t)sys_stub,   //SYS_MUNLOCKALL
        [153] = (syscall_t)sys_stub,   //SYS_VHANGUP
        [154] = (syscall_t)sys_stub,   //SYS_MODIFY_LDT
        [155] = (syscall_t)sys_stub,   //SYS_PIVOT_ROOT
        [156] = (syscall_t)sys_stub,   //SYS__SYSCTL
        [157] = (syscall_t)sys_stub,   //SYS_PRCTL
        [158] = (syscall_t)sys_stub,   //SYS_ARCH_PRCTL
        [159] = (syscall_t)sys_stub,   //SYS_ADJTIMEX
        [160] = (syscall_t)sys_stub,   //SYS_SETRLIMIT
        [161] = (syscall_t)sys_stub,   //SYS_CHROOT
        [162] = (syscall_t)sys_stub,   //SYS_SYNC
        [163] = (syscall_t)sys_stub,   //SYS_ACCT
        [164] = (syscall_t)sys_stub,   //SYS_SETTIMEOFDAY
        [165] = (syscall_t)sys_stub,   //SYS_MOUNT
        [166] = (syscall_t)sys_stub,   //SYS_UMOUNT2
        [167] = (syscall_t)sys_stub,   //SYS_SWAPON
        [168] = (syscall_t)sys_stub,   //SYS_SWAPOFF
        [169] = (syscall_t)sys_stub,   //SYS_REBOOT
        [170] = (syscall_t)sys_stub,   //SYS_SETHOSTNAME
        [171] = (syscall_t)sys_stub,   //SYS_SETDOMAINNAME
        [172] = (syscall_t)sys_stub,   //SYS_IOPL
        [173] = (syscall_t)sys_stub,   //SYS_IOPERM
        [174] = (syscall_t)sys_stub,   //SYS_CREATE_MODULE
        [175] = (syscall_t)sys_stub,   //SYS_INIT_MODULE
        [176] = (syscall_t)sys_stub,   //SYS_DELETE_MODULE
        [177] = (syscall_t)sys_stub,   //SYS_GET_KERNEL_SYMS
        [178] = (syscall_t)sys_stub,   //SYS_QUERY_MODULE
        [179] = (syscall_t)sys_stub,   //SYS_QUOTACTL
        [180] = (syscall_t)sys_stub,   //SYS_NFSSERVCTL
        [181] = (syscall_t)sys_stub,   //SYS_GETPMSG
        [182] = (syscall_t)sys_stub,   //SYS_PUTPMSG
        [183] = (syscall_t)sys_stub,   //SYS_AFS_SYSCALL
        [184] = (syscall_t)sys_stub,   //SYS_TUXCALL
        [185] = (syscall_t)sys_stub,   //SYS_SECURITY
        [186] = (syscall_t)sys_stub,   //SYS_GETTID
        [187] = (syscall_t)sys_stub,   //SYS_READAHEAD
        [188] = (syscall_t)sys_stub,   //SYS_SETXATTR
        [189] = (syscall_t)sys_stub,   //SYS_LSETXATTR
        [190] = (syscall_t)sys_stub,   //SYS_FSETXATTR
        [191] = (syscall_t)sys_stub,   //SYS_GETXATTR
        [192] = (syscall_t)sys_stub,   //SYS_LGETXATTR
        [193] = (syscall_t)sys_stub,   //SYS_FGETXATTR
        [194] = (syscall_t)sys_stub,   //SYS_LISTXATTR
        [195] = (syscall_t)sys_stub,   //SYS_LLISTXATTR
        [196] = (syscall_t)sys_stub,   //SYS_FLISTXATTR
        [197] = (syscall_t)sys_stub,   //SYS_REMOVEXATTR
        [198] = (syscall_t)sys_stub,   //SYS_LREMOVEXATTR
        [199] = (syscall_t)sys_stub,   //SYS_FREMOVEXATTR
        [200] = (syscall_t)sys_stub,   //SYS_TKILL
        [201] = (syscall_t)sys_stub,   //SYS_TIME
        [202] = (syscall_t)sys_futex,  //SYS_FUTEX
        [203] = (syscall_t)sys_stub,   //SYS_SCHED_SETAFFINITY
        [204] = (syscall_t)sys_stub,   //SYS_SCHED_GETAFFINITY
        [205] = (syscall_t)sys_stub,   //SYS_SET_THREAD_AREA
        [206] = (syscall_t)sys_stub,   //SYS_IO_SETUP
        [207] = (syscall_t)sys_stub,   //SYS_IO_DESTROY
        [208] = (syscall_t)sys_stub,   //SYS_IO_GETEVENTS
        [209] = (syscall_t)sys_stub,   //SYS_IO_SUBMIT
        [210] = (syscall_t)sys_stub,   //SYS_IO_CANCEL
        [211] = (syscall_t)sys_stub,   //SYS_GET_THREAD_AREA
        [212] = (syscall_t)sys_stub,   //SYS_LOOKUP_DCOOKIE
        [213] = (syscall_t)sys_stub,   //SYS_EPOLL_CREATE
        [214] = (syscall_t)sys_stub,   //SYS_EPOLL_CTL_OLD
        [215] = (syscall_t)sys_stub,   //SYS_EPOLL_WAIT_OLD
        [216] = (syscall_t)sys_stub,   //SYS_REMAP_FILE_PAGES
        [217] = (syscall_t)sys_stub,   //SYS_GETDENTS64
        [218] = (syscall_t)sys_stub,   //SYS_SET_TID_ADDRESS
        [219] = (syscall_t)sys_stub,   //SYS_RESTART_SYSCALL
        [220] = (syscall_t)sys_stub,   //SYS_SEMTIMEDOP
        [221] = (syscall_t)sys_stub,   //SYS_FADVISE64
        [222] = (syscall_t)sys_stub,   //SYS_TIMER_CREATE
        [223] = (syscall_t)sys_stub,   //SYS_TIMER_SETTIME
        [224] = (syscall_t)sys_stub,   //SYS_TIMER_GETTIME
        [225] = (syscall_t)sys_stub,   //SYS_TIMER_GETOVERRUN
        [226] = (syscall_t)sys_stub,   //SYS_TIMER_DELETE
        [227] = (syscall_t)sys_stub,   //SYS_CLOCK_SETTIME
        [228] = (syscall_t)sys_stub,   //SYS_CLOCK_GETTIME
        [229] = (syscall_t)sys_stub,   //SYS_CLOCK_GETRES
        [230] = (syscall_t)sys_stub,   //SYS_CLOCK_NANOSLEEP
        [231] = (syscall_t)sys_exit_group, //SYS_EXIT_GROUP
        [232] = (syscall_t)sys_stub,   //SYS_EPOLL_WAIT
        [233] = (syscall_t)sys_stub,   //SYS_EPOLL_CTL
        [234] = (syscall_t)sys_stub,   //SYS_TGKILL
        [235] = (syscall_t)sys_stub,   //SYS_UTIMES
        [236] = (syscall_t)sys_stub,   //SYS_VSERVER
        [237] = (syscall_t)sys_stub,   //SYS_MBIND
        [238] = (syscall_t)sys_stub,   //SYS_SET_MEMPOLICY
        [239] = (syscall_t)sys_stub,   //SYS_GET_MEMPOLICY
        [240] = (syscall_t)sys_stub,   //SYS_MQ_OPEN
        [241] = (syscall_t)sys_stub,   //SYS_MQ_UNLINK
        [242] = (syscall_t)sys_stub,   //SYS_MQ_TIMEDSEND
        [243] = (syscall_t)sys_stub,   //SYS_MQ_TIMEDRECEIVE
        [244] = (syscall_t)sys_stub,   //SYS_MQ_NOTIFY
        [245] = (syscall_t)sys_stub,   //SYS_MQ_GETSETATTR
        [246] = (syscall_t)sys_stub,   //SYS_KEXEC_LOAD
        [247] = (syscall_t)sys_stub,   //SYS_WAITID
        [248] = (syscall_t)sys_stub,   //SYS_ADD_KEY
        [249] = (syscall_t)sys_stub,   //SYS_REQUEST_KEY
        [250] = (syscall_t)sys_stub,   //SYS_KEYCTL
        [251] = (syscall_t)sys_stub,   //SYS_IOPRIO_SET
        [252] = (syscall_t)sys_stub,   //SYS_IOPRIO_GET
        [253] = (syscall_t)sys_stub,   //SYS_INOTIFY_INIT
        [254] = (syscall_t)sys_stub,   //SYS_INOTIFY_ADD_WATCH
        [255] = (syscall_t)sys_stub,   //SYS_INOTIFY_RM_WATCH
        [256] = (syscall_t)sys_stub,   //SYS_MIGRATE_PAGES
        [257] = (syscall_t)sys_stub,   //SYS_OPENAT
        [258] = (syscall_t)sys_stub,   //SYS_MKDIRAT
        [259] = (syscall_t)sys_stub,   //SYS_MKNODAT
        [260] = (syscall_t)sys_stub,   //SYS_FCHOWNAT
        [261] = (syscall_t)sys_stub,   //SYS_FUTIMESAT
        [262] = (syscall_t)sys_stub,   //SYS_NEWFSTATAT
        [263] = (syscall_t)sys_stub,   //SYS_UNLINKAT
        [264] = (syscall_t)sys_stub,   //SYS_RENAMEAT
        [265] = (syscall_t)sys_stub,   //SYS_LINKAT
        [266] = (syscall_t)sys_stub,   //SYS_SYMLINKAT
        [267] = (syscall_t)sys_stub,   //SYS_READLINKAT
        [268] = (syscall_t)sys_stub,   //SYS_FCHMODAT
        [269] = (syscall_t)sys_stub,   //SYS_FACCESSAT
        [270] = (syscall_t)sys_stub,   //SYS_PSELECT6
        [271] = (syscall_t)sys_ppoll,   //SYS_PPOLL
};

void restart_syscall(regs_t* regs, int signum) {
    if(get_current_process()->syscall > 0 && regs->rax == -ERESTART) {
        if(signum == SIGCONT || (get_current_process()->signalHandlers[signum].sa_flags & SA_RESTART)) {
            regs->rax = get_current_process()->syscall;
            get_current_process()->syscall = 0;
            syscall_entry(regs);
        } else {
            get_current_process()->syscall = 0;
            regs->rax = -EINTR;
        }
    }
}

void syscall_entry(regs_t* regs) {
    uintptr_t syscallNo = regs->rax;

    if(syscall_table[syscallNo]) {
        get_current_process()->saved_registers = regs;

        int returnCode = syscall_table[syscallNo](regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);

        if(syscallNo == 15) {
            return;
        }

        regs->rax = returnCode;
        return;
    }

    regs->rax = -1;
}

void syscall_init() {
    futex_queue = ht_create(32);
    futex_lock = calloc(1, sizeof(spin_t));
    spin_unlock(futex_lock);
}