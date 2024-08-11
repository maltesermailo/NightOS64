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
#include "../idt.h"
#include <signal.h>

#define PROC_FLAG_KERNEL 1<<0
#define PROC_FLAG_RUNNING 1<<1 // whether the process is currently running
#define PROC_FLAG_ON_CPU 1<<2 // whether the process is currently on the cpu
#define PROC_FLAG_SLEEP_INTERRUPTIBLE 1<<3 // interruptable process
#define PROC_FLAG_SLEEP_NON_INTERRUPTIBLE 1<<4 //non interruptable
#define PROC_FLAG_FINISHED 1<<5
/**
 * System server is a special case process.
 * There exists only one on the entire system in userspace and it holds various services for the kernel
 * The system server always has full access to the machine and all resources, but runs in userspace
 * A system server can't be manipulated by user-space processes.
 * User-space processes can only communicate using the syscall SYS_SRVCTL
 *
 * Non-exhaustive list of System Server:
 * - Internal File System Drivers
 * - Device Drivers(e.g. audio, network and more)
 */
#define PROC_FLAG_SYSTEM_SERVER 1<<6
/**
 * A trusted server is a special case process.
 * It is a server that can be trusted with access to hardware for the user context
 */
#define PROC_FLAG_TRUSTED_SERVER 1<<7
/**
 * Server is a service process
 * A server can be any user process.
 * It doesn't have access to memory mapped I/O
 */
#define PROC_FLAG_SERVER 1<<8

#define CLONE_VM 0x00000100
#define CLONE_FS 0x00000200
#define CLONE_FILES	0x00000400
#define CLONE_SIGHAND 0x00000800
#define CLONE_PTRACE 0x00002000
#define CLONE_VFORK 0x00004000
#define CLONE_PARENT 0x00008000
#define CLONE_THREAD 0x00010000
#define CLONE_NEWNS 0x00020000
#define CLONE_SYSVSEM 0x00040000
#define CLONE_SETTLS 0x00080000
#define CLONE_PARENT_SETTID 0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_DETACHED 0x00400000
#define CLONE_UNTRACED 0x00800000
#define CLONE_CHILD_SETTID 0x01000000
#define CLONE_NEWCGROUP 0x02000000
#define CLONE_NEWUTS 0x04000000
#define CLONE_NEWIPC 0x08000000
#define CLONE_NEWUSER 0x10000000
#define CLONE_NEWPID 0x20000000
#define CLONE_NEWNET 0x40000000
#define CLONE_IO 0x80000000

typedef int pid_t;

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

    atomic_int process_count;
    spin_t lock;
} fd_table_t;

typedef struct {
    uintptr_t page_directory;
    unsigned long heap; //Current program break

    atomic_int process_count; //Count of threads still existent.
    spin_t lock;
} mm_struct_t;

typedef struct SignalHandler {
    void (*handler)(int);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
} signal_handler_t;

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

    signal_handler_t signalHandlers[32]; //POSIX defines 32 signals which will be implemented

    regs_t* saved_registers;
    unsigned long syscall; //Syscall if interrupted

    fd_table_t* fd_table;

    sigset_t blocked_signals;
    sigset_t pending_signals;

    spin_t lock;
} process_t;

typedef struct process_control_block {
    volatile process_t* current_process;
    process_t* kernel_idle_process;

    int core;

    uintptr_t current_page_map;
} pcb_t;

struct clone_args {
    uint64_t flags;        /* Flags bit mask */
    uint64_t pidfd;        /* Where to store PID file descriptor
                                    (int *) */
    uint64_t child_tid;    /* Where to store child TID,
                                    in child's memory (pid_t *) */
    uint64_t parent_tid;   /* Where to store child TID,
                                    in parent's memory (pid_t *) */
    uint64_t exit_signal;  /* Signal to deliver to parent on
                                    child termination */
    uint64_t stack;        /* Pointer to lowest byte of stack */
    uint64_t stack_size;   /* Size of stack */
    uint64_t tls;          /* Location of new TLS */
    uint64_t set_tid;      /* Pointer to a pid_t array
                                    (since Linux 5.5) */
    uint64_t set_tid_size; /* Number of elements in set_tid
                                    (since Linux 5.5) */
    uint64_t cgroup;       /* File descriptor for target cgroup
                                    of child (since Linux 5.7) */
};

void process_init();

//Process creation functions
void process_create_task(char* path, bool is_kernel); //used by the kernel at load to create the init task
void process_create_thread(void* address); //Creates kernel thread
void process_create_idle();
pid_t process_fork();
pid_t process_clone(struct clone_args* args, size_t size);

//Process management functions
void process_thread_exit(int retval);
void process_exit(int retval);
void process_set_signal_handler(int signum, struct sigaction* sigaction);
signal_handler_t* process_get_signal_handler(int signum);
void process_check_signals(regs_t* regs);
void process_enter_signal(regs_t* regs, int signum);
int process_signal_return();

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
void sleep(long milliseconds);
void wakeup_sleeping();
bool wakeup_now(process_t* proc);

#endif //NIGHTOS_PROCESS_H
