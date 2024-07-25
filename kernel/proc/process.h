//
// Created by Jannik on 03.04.2024.
//

#pragma once
#ifndef NIGHTOS_PROCESS_H
#define NIGHTOS_PROCESS_H
#include <stdint.h>
#include "../fs/vfs.h"
#include "../lock.h"
#include "../mutex.h"

#define PROC_FLAG_KERNEL 1<<0
#define PROC_FLAG_RUNNING 1<<1 // whether the process is currently running
#define PROC_FLAG_ON_CPU 1<<2 // whether the process is currently on the cpu
#define PROC_FLAG_SLEEP_INTERRUPTIBLE 1<<3 // interruptable process
#define PROC_FLAG_SLEEP_NON_INTERRUPTIBLE 1<<4 //non interruptable
#define PROC_FLAG_FINISHED 1<<5

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

    uintptr_t kernel_stack;
    uintptr_t user_stack;
    struct process* process;
} kernel_thread_t;

typedef struct file_descriptor_table {
    int capacity; // Current max
    int length; // Current length

    file_handle_t** handles; // Array of file nodes

    spin_t lock;
} fd_table_t;

typedef struct {
    uintptr_t page_directory;
    unsigned long heap; //Current program break

    atomic_int process_count; //Count of threads still existent.
    spin_t lock;
} mm_struct_t;

typedef struct process {
    pid_t id;

    int uid;
    int gid;
    int cpu; //the current cpu

    pid_t tgid; //process group, this is the pid of the parent process for each process. we use that since we want to be compatible with linux apps
    pid_t parent;

    int flags;

    int status;
    uint64_t sleepTick; //The tick until the process sleeps

    mm_struct_t* page_directory;
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
void process_create_task(char* path, bool is_kernel); //used by the kernel at load to create the init task
void process_create_thread(void* address); //Creates kernel thread
void process_create_idle();
pid_t process_fork();

//Process management functions
void process_exit(int retval);

//Process memory functions
uintptr_t process_get_current_pml();
void process_set_current_pml(uintptr_t pml);

//Process file management functions
int process_open_fd(file_node_t* node, int mode);
void process_close_fd(int fd);

//Current process state
process_t* get_current_process();

int execve(char* path, char** argv, char** envp);

//Scheduler
void schedule_process(process_t* process);
void schedule(bool sleep);

void wait_for_object(mutex_t* mutex);
void wakeup_waiting(list_t* queue);

/***
 * This method waits until x milliseconds passed
 * @param milliseconds the amount of milliseconds passed since call
 */
void sleep(int milliseconds);
void wakeup_sleeping();

#endif //NIGHTOS_PROCESS_H
