//
// Created by Jannik on 03.04.2024.
//

#ifndef NIGHTOS_PROCESS_H
#define NIGHTOS_PROCESS_H
#include <stdint.h>
#include "../fs/vfs.h"
#include "../lock.h"

typedef unsigned long long pid_t;

struct process;

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

    int tid;
    int priority;
    process* process;
} kernel_thread_t;

typedef struct file_handle {
    file_node_t* fileNode;
    int mode;
    uint64_t offset;
} file_handle_t;

typedef struct file_descriptor_table {
    int capacity; // Current max
    int length; // Current length

    file_node_t** handles; // Array of file nodes

    spin_t lock;
} fd_table_t;

typedef struct process {
    int id;

    int uid;
    int gid;

    uintptr_t page_directory;
    kernel_thread_t main_thread; // this is the thread that started the process, if it is killed, the process is dead and all threads are killed

    fd_table_t* fd_table;
} process_t;

typedef struct process_control_block {
    volatile process_t* current_process;
    process_t* kernel_idle_process;

    int core;

    uintptr_t current_page_map;
} pcb_t;

void process_init();

//Process creation functions
void process_create_init(); //Loads the init file into memory and calls process_create_task with the start address
void process_create_task(void* start_address);
pid_t process_fork();

//Process management functions
void process_exit(int retval);

//Process memory functions
uintptr_t process_get_current_pml();
void process_set_current_pml(uintptr_t pml);

//Current process state
process_t* get_current_process();

//Scheduler
void switch_task();

#endif //NIGHTOS_PROCESS_H
