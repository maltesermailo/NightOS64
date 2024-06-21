//
// Created by Jannik on 04.04.2024.
//
#include "process.h"
#include "../alloc.h"
#include "../memmgr.h"
#include "../terminal.h"
#include "../../libc/include/kernel/list.h"
static struct process_control_block pcb; //currently the only one, we're running single core
list_t* process_list;

extern void longjmp(kernel_thread_t* thread);
extern void enter_user(uintptr_t rip, uintptr_t rsp);
extern void enter_kernel(uintptr_t rip, uintptr_t rsp);

/**
 * Small internal function to copy a mini function from the kernel to another location accessible by user space
 * @param address the address
 * @param len the size
 */
void* copy_app_memory(void* address, size_t len, bool is_kernel) {
    void* memory = mmap(0, len, is_kernel);

    printf("Copied %d bytes to 0x%x\n", len, memory);

    memcpy(memory, address, len);

    return memory;
}

void process_create_task(void* address, bool is_kernel) {
    process_t* process = calloc(1, sizeof(process_t));

    process->id = 1;
    process->page_directory = kalloc_frame();

    memmgr_clone_page_map(memmgr_get_current_pml4(), memmgr_get_from_physical(process->page_directory));
    load_page_map(process->page_directory);

    pcb.core = 0;
    pcb.current_page_map = process->page_directory;
    pcb.current_process = process;
    pcb.kernel_idle_process = 0;

    process->main_thread.process = 1;
    process->main_thread.priority = 0;
    process->main_thread.rip = copy_app_memory(address, 4096, is_kernel);
    process->main_thread.rsp = mmap(0, 16384, is_kernel);

    process->uid = 0;
    process->gid = 0;
    process->flags = is_kernel ? PROC_FLAG_KERNEL : 0;
    process->flags |= PROC_FLAG_RUNNING;

    process->fd_table = calloc(1, sizeof(fd_table_t));
    process->fd_table->capacity = 32;
    process->fd_table->length = 0;
    process->fd_table->handles = malloc(sizeof(file_node_t*) * process->fd_table->capacity);
    memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->capacity);
    spin_unlock(&process->fd_table->lock);

    list_insert(process_list, process);

    printf("Executing at 0x%x with stack 0x%x\n", process->main_thread.rip, process->main_thread.rsp);

    if(is_kernel) {
        enter_kernel(process->main_thread.rip, process->main_thread.rsp);
    } else {
        enter_user(process->main_thread.rip, process->main_thread.rsp);
    }
}

uintptr_t process_get_current_pml() {
    return pcb.current_page_map;
}

void process_set_current_pml(uintptr_t pml) {
    pcb.current_page_map = pml;
}

int process_open_fd(file_node_t* node, int mode) {
    process_t* current = get_current_process();

    if(current->fd_table->length >= current->fd_table->capacity) {
        return -1; //no space, we will increase that later
    }

    spin_lock(&current->fd_table->lock);
    int index = 0;

    for(int i = 0; i < current->fd_table->capacity; i++) {
        if(current->fd_table->handles[i] == NULL) {
            index = i;
        }
    }

    file_handle_t* handle = calloc(1, sizeof(file_handle_t));
    handle->fileNode = node;
    handle->offset = 0;
    handle->mode = mode;

    current->fd_table->handles[index] = handle;
    current->fd_table->length++;
    spin_unlock(&current->fd_table->lock);

    return index;
}

void process_close_fd(int fd) {
    process_t* current = get_current_process();

    spin_lock(&current->fd_table->lock);

    file_handle_t* handle = current->fd_table->handles[fd];

    if(handle->fileNode->file_ops.close) {
        handle->fileNode->file_ops.close(handle->fileNode); //Signal file system driver to flush
    }

    free(handle);

    current->fd_table->handles[fd] = NULL;
    current->fd_table->length--;
    spin_unlock(&current->fd_table->lock);
}

process_t* get_current_process() {
    return pcb.current_process;
}