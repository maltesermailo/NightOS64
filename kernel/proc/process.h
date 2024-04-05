//
// Created by Jannik on 03.04.2024.
//

#ifndef NIGHTOS_PROCESS_H
#define NIGHTOS_PROCESS_H
#include <stdint.h>

typedef struct kernel_thread {
    //Important pointers
    uintptr_t rsp;
    uintptr_t rbp;
    uintptr_t rip;
    //Thread-Local Storage
    uintptr_t tls_base;

    //General registers
    uintptr_t rbx;
    uintptr_t rdi;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;

    int priority;
    int process;
} kernel_thread_t;

typedef struct process {
    int id;

    uintptr_t page_directory;
    kernel_thread_t main_thread;
} process_t;

typedef struct process_control_block {
    volatile process_t* current_process;
    process_t* kernel_idle_process;

    int core;

    uintptr_t current_page_map;
};

void process_create_task(void* start_address);
uintptr_t process_get_current_pml();
void process_set_current_pml(uintptr_t pml);

#endif //NIGHTOS_PROCESS_H
