//
// Created by Jannik on 04.04.2024.
//
#include "process.h"
#include "../alloc.h"
#include "../memmgr.h"
static struct process_control_block pcb;

extern void longjmp(kernel_thread_t* thread);
extern void enter_user(uintptr_t rip, uintptr_t rsp);

void process_create_task(void* address) {
    process_t* process = calloc(1, sizeof(process_t));

    process->id = 1;
    process->page_directory = malloc(4096);

    memmgr_clone_page_map(memmgr_get_current_pml4(), process->page_directory);
    load_page_map(pcb.current_page_map);

    process->main_thread.process = 1;
    process->main_thread.priority = 0;
    process->main_thread.rip = (uintptr_t)address;
    process->main_thread.rsp = mmap(0, 16384, false);

    pcb.core = 0;
    pcb.current_page_map = process->page_directory;
    pcb.current_process = process;
    pcb.kernel_idle_process = 0;

    printf("Executing at 0x%x with stack 0x%x\n", address, process->main_thread.rsp);

    enter_user(process->main_thread.rip, process->main_thread.rsp);
}