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
    memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->handles);
    process->fd_table->lock = ATOMIC_FLAG_INIT;

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

process_t* get_current_process() {
    return pcb.current_process;
}