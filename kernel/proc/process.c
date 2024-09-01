//
// Created by Jannik on 04.04.2024.
//
#include "process.h"
#include "../alloc.h"
#include "../memmgr.h"
#include "../terminal.h"
#include "../../libc/include/kernel/list.h"
#include "../gdt.h"
#include "../../libc/include/kernel/tree.h"
#include "../program/elf.h"
#include "../timer.h"
#include <signal.h>
#include <string.h>

#define PUSH_PTR(stack, type, value) { \
            stack -= sizeof(type);     \
            while (stack & (sizeof(type)-1)) stack--; \
            *((type*)stack) = (value);\
}

#define POP_PTR(stack, type, value) { \
            *((type*)stack) = (value); \
            stack += sizeof(type);     \
}

char* push_string_to_userstack(uintptr_t* stack, const char* str) {
    size_t len = strlen(str) + 1;  // Include null terminator

    // Decrease stack and align to 8 bytes
    *stack -= len;
    *stack &= ~7ULL;

    // Copy the string to the stack
    memcpy((void*)*stack, str, len);
}

static struct process_control_block pcb; //currently the only one, we're running single core
static int id_generator = 1;
list_t* process_list;
tree_t* process_tree;
list_t* thread_queue; //Queue of threads for the scheduler
list_t* sleeping_queue;

spin_t* sleep_lock; //Lock for the sleep queue
spin_t* process_lock; //Lock for the process list and tree
spin_t* queue_lock; //Lock for the scheduler queue

extern void longjmp(kernel_thread_t* thread);
extern int setjmp(kernel_thread_t* thread);
extern void enter_user(uintptr_t rip, uintptr_t rsp);
extern void enter_kernel(uintptr_t rip, uintptr_t rsp);
extern void enter_user_2(uintptr_t rip, uintptr_t rsp);

_Noreturn void idle() {
    while(1) {
        __asm__ volatile("sti"); //Enable interrupts while waiting.
        __asm__ volatile("hlt");
        __asm__ volatile("cli");
        schedule(false);
    }
}

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

void process_init() {
    sleep_lock = calloc(1, sizeof(spin_t));
    process_lock = calloc(1, sizeof(spin_t));
    queue_lock = calloc(1, sizeof(spin_t));

    spin_unlock(sleep_lock);
    spin_unlock(process_lock);
    spin_unlock(queue_lock);

    process_list = list_create();
    process_tree = tree_create();
    thread_queue = list_create();
    sleeping_queue = list_create();
}

void process_create_task(char* path, bool is_kernel) {
    file_node_t* node = open(path, 0);

    if(node == NULL) {
        printf("Error: can't open %s\n", path);
        panic();
        return;
    }

    file_handle_t* handle = create_handle(node);

    process_t* process = calloc(1, sizeof(process_t));

    spin_unlock(&process->lock);

    process->id = 1;
    process->tgid = 1;

    process->page_directory = calloc(1, sizeof(mm_struct_t));
    process->page_directory->process_count = 1;
    process->page_directory->page_directory = 0x1000;
    spin_unlock(&process->page_directory->lock);

    //memmgr_clone_page_map(memmgr_get_current_pml4(), memmgr_get_from_physical(process->page_directory->page_directory));
    //load_page_map(process->page_directory->page_directory);

    pcb.core = 0;
    pcb.current_page_map = process->page_directory->page_directory;
    pcb.current_process = process;

    elf_t* elf = load_elf(handle);
    if(exec_elf(elf, 0, 0, 0)) {
        printf("Error while loading elf file.\n");
        return;
    }

    process->main_thread.process = process;
    process->main_thread.priority = 0;
    process->main_thread.user_stack = (uintptr_t) (mmap(0, 16384, false) + 16384);
    process->main_thread.kernel_stack = (uintptr_t) (malloc(16384) + 16384);
    process->main_thread.rip = (uintptr_t)elf->entrypoint;

    process->uid = 0;
    process->gid = 0;
    process->flags = is_kernel ? PROC_FLAG_KERNEL : 0;
    process->flags |= PROC_FLAG_RUNNING;
    process->cwd_file = open("/", 0);
    //Always make sure to clean this string up!!!
    process->cwd = strdup("/");

    process->fd_table = calloc(1, sizeof(fd_table_t));
    process->fd_table->capacity = 32;
    process->fd_table->length = 0;
    process->fd_table->handles = malloc(sizeof(file_node_t*) * process->fd_table->capacity);
    memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->capacity);
    spin_unlock(&process->fd_table->lock);

    file_node_t* console = open("/dev/tty", 0);
    process_open_fd(console, 0);
    process_open_fd(console, 0);
    process_open_fd(console, 0);

    set_stack_pointer(process->main_thread.kernel_stack);

    list_insert(process_list, process);
    tree_insert_child(process_tree, NULL, process);

    unsigned long userStack = process->main_thread.user_stack;

    printf("Before 0x%x\n", userStack);

    push_string_to_userstack(&userStack, "PATH=/usr/bin");
    uintptr_t envp = userStack;

    PUSH_PTR(userStack, uintptr_t, 0); //ENVP ZERO
    PUSH_PTR(userStack, uintptr_t, envp);
    PUSH_PTR(userStack, uintptr_t, 0); //ARGV ZERO
    PUSH_PTR(userStack, uintptr_t, 0); //ARGC

    printf("After 0x%x\n", userStack);

    process->main_thread.user_stack = userStack;
    process->main_thread.rsp = is_kernel ? process->main_thread.kernel_stack : process->main_thread.user_stack;

    printf("Executing at 0x%x with stack 0x%x\n", process->main_thread.rip, process->main_thread.rsp);

    if(is_kernel) {
        enter_kernel(process->main_thread.rip, process->main_thread.rsp);
    } else {
        enter_user(process->main_thread.rip, process->main_thread.rsp);
    }
}

void process_create_idle() {
    process_t* process = calloc(1, sizeof(process_t));

    spin_unlock(&process->lock);

    process->id = 0;
    process->tgid = 0;
    process->page_directory = calloc(1, sizeof(mm_struct_t));
    process->page_directory->process_count = 1;
    process->page_directory->page_directory = (uintptr_t)memmgr_get_current_pml4();
    spin_unlock(&process->page_directory->lock);

    pcb.kernel_idle_process = process;

    process->main_thread.process = process;
    process->main_thread.priority = 0;
    process->main_thread.rip = (uintptr_t) &idle;
    process->main_thread.kernel_stack = (uintptr_t) malloc(16384);
    process->main_thread.rsp = process->main_thread.kernel_stack;

    process->uid = 0;
    process->gid = 0;
    process->flags = PROC_FLAG_KERNEL | PROC_FLAG_RUNNING;

    process->fd_table = calloc(1, sizeof(fd_table_t));
    process->fd_table->capacity = 8;
    process->fd_table->length = 0;
    process->fd_table->handles = malloc(sizeof(file_node_t*) * process->fd_table->capacity);
    memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->capacity);
    spin_unlock(&process->fd_table->lock);
}

extern void* fork_exit;

pid_t process_fork() {
    process_t* process = calloc(1, sizeof(process_t));
    process_t* parent = get_current_process();

    spin_unlock(&process->lock);

    process->id = ++id_generator;
    process->parent = parent->id;
    process->tgid = process->id;
    process->page_directory = calloc(1, sizeof(mm_struct_t));
    process->page_directory->process_count = 1;
    process->page_directory->page_directory = kalloc_frame();
    spin_unlock(&process->page_directory->lock);

    memmgr_clone_page_map(memmgr_get_current_pml4(), memmgr_get_from_physical(process->page_directory->page_directory));

    process->main_thread.process = process;
    process->main_thread.priority = parent->main_thread.priority;

    //Save parent process state
    setjmp(&process->main_thread);
    process->main_thread.user_stack = parent->main_thread.user_stack;
    process->main_thread.kernel_stack = (uintptr_t) (malloc(16384) + 16384);
    process->main_thread.rip = (uintptr_t) fork_exit;
    process->main_thread.rsp = process->main_thread.user_stack;

    regs_t registers;
    memcpy(&registers, process->saved_registers, sizeof(regs_t));
    registers.rax = 0;
    PUSH_PTR(process->main_thread.kernel_stack, regs_t, registers);

    process->uid = parent->uid;
    process->gid = parent->gid;
    process->flags = parent->flags;
    process->flags &= ~PROC_FLAG_RUNNING;
    process->flags &= ~PROC_FLAG_ON_CPU;

    process->fd_table = calloc(1, sizeof(fd_table_t));
    process->fd_table->capacity = parent->fd_table->capacity;
    process->fd_table->length = parent->fd_table->length;
    process->fd_table->handles = malloc(sizeof(file_node_t*) * process->fd_table->capacity);
    memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->capacity);

    for(int i = 0; i < process->fd_table->capacity; i++) {
        if(parent->fd_table->handles[i] != NULL) {
            file_handle_t* parentHandle = parent->fd_table->handles[i];

            file_handle_t* handle = calloc(1, sizeof(file_handle_t*));
            handle->fileNode = parentHandle->fileNode;
            handle->offset = parentHandle->offset;
            handle->mode = parentHandle->mode;

            process->fd_table->handles[i] = handle;
        }
    }

    spin_unlock(&process->fd_table->lock);

    list_insert(process_list, process);
    tree_insert_child(process_tree, NULL, process);
    list_insert(thread_queue, process);

    return process->id;
}

pid_t process_clone(struct clone_args* args, size_t size) {
    process_t* process = calloc(1, sizeof(process_t));
    process_t* parent = get_current_process();

    spin_unlock(&process->lock);

    process->id = ++id_generator;
    process->parent = parent->id;

    if(args->flags & CLONE_THREAD) {
        process->tgid = parent->tgid;
    }

    if(args->flags & CLONE_VM) {
        process->page_directory = parent->page_directory;

        spin_lock(&process->page_directory->lock);
        process->page_directory->process_count++;
        spin_unlock(&process->page_directory->lock);
    } else {
        process->page_directory = calloc(1, sizeof(mm_struct_t));
        process->page_directory->process_count = 1;
        process->page_directory->page_directory = kalloc_frame();
        spin_unlock(&process->page_directory->lock);

        memmgr_clone_page_map(memmgr_get_current_pml4(), memmgr_get_from_physical(process->page_directory->page_directory));
    }

    process->main_thread.process = process;
    process->main_thread.priority = parent->main_thread.priority;

    //Save parent process state
    setjmp(&process->main_thread);
    process->main_thread.user_stack = parent->main_thread.user_stack;
    process->main_thread.kernel_stack = (uintptr_t) (malloc(16384) + 16384);
    process->main_thread.rip = (uintptr_t) fork_exit;
    process->main_thread.rsp = process->main_thread.user_stack;

    regs_t registers;
    memcpy(&registers, process->saved_registers, sizeof(regs_t));
    registers.rax = 0;
    PUSH_PTR(process->main_thread.kernel_stack, regs_t, registers);

    process->uid = parent->uid;
    process->gid = parent->gid;
    process->flags = parent->flags;
    process->flags &= ~PROC_FLAG_RUNNING;
    process->flags &= ~PROC_FLAG_ON_CPU;

    if(args->flags & CLONE_FILES) {
        process->fd_table = parent->fd_table;
        spin_lock(&process->fd_table->lock);
        process->fd_table->process_count++;
        spin_unlock(&process->fd_table->lock);
    } else {
        process->fd_table = calloc(1, sizeof(fd_table_t));
        process->fd_table->capacity = parent->fd_table->capacity;
        process->fd_table->length = parent->fd_table->length;
        process->fd_table->handles = malloc(sizeof(file_node_t*) * process->fd_table->capacity);
        memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->capacity);

        for(int i = 0; i < process->fd_table->capacity; i++) {
            if(parent->fd_table->handles[i] != NULL) {
                file_handle_t* parentHandle = parent->fd_table->handles[i];

                file_handle_t* handle = calloc(1, sizeof(file_handle_t*));
                handle->fileNode = parentHandle->fileNode;
                handle->offset = parentHandle->offset;
                handle->mode = parentHandle->mode;

                process->fd_table->handles[i] = handle;
            }
        }

        spin_unlock(&process->fd_table->lock);
    }

    if(args->flags & CLONE_SETTLS) {
        process->main_thread.tls_base = args->tls;
    }

    if(args->parent_tid != 0) {
        *((unsigned long*) args->parent_tid) = process->id;
    }

    list_insert(process_list, process);
    tree_insert_child(process_tree, NULL, process);
    list_insert(thread_queue, process);

    return process->id;
}

uintptr_t process_get_current_pml() {
    return pcb.current_page_map;
}

void process_set_current_pml(uintptr_t pml) {
    pcb.current_page_map = pml;
}

void process_free_pml(uintptr_t pml) {
    memmgr_clear_page_map(pml);
}

int execve(char* path, char** argv, char** envp) {
    process_t* process = get_current_process();

    if(process == NULL) return -1;

    if(process->tgid != 0) {
        return -2; //Can't replace image in thread
    }

    file_node_t* node = open(path, 0);

    if(node == NULL) {
        printf("Error: can't open %s\n", path);
        return -1;
    }

    file_handle_t* handleElf = create_handle(node);

    spin_lock(&process->page_directory->lock);
    if(process->page_directory->process_count == 1) {
        process_free_pml(process->page_directory->page_directory);

        free(process->page_directory);
    }

    process->page_directory = calloc(1, sizeof(mm_struct_t));
    process->page_directory->process_count = 1;
    process->page_directory->page_directory = kalloc_frame();
    spin_unlock(&process->page_directory->lock);
    spin_lock(&process->page_directory->lock);

    memmgr_clone_page_map((uint64_t *) 0x1000, (uint64_t *) process->page_directory->page_directory); //Clone from init pml
    load_page_map(process->page_directory->page_directory);

    process->main_thread.user_stack = (uintptr_t) (mmap(0, 16384, false) + 16384);

    process->flags = PROC_FLAG_RUNNING;

    if(process->fd_table->length > 0) {
        for(int i = 0; i < process->fd_table->capacity; i++) {
            file_handle_t* handle = process->fd_table->handles[i];

            if(handle->fileNode->file_ops.close) {
                handle->fileNode->file_ops.close(handle->fileNode); //Signal file system driver to flush
            }

            free(handle);

            process->fd_table->handles[i] = NULL;
            process->fd_table->length--;
        }

        free(process->fd_table->handles);
        free(process->fd_table);
    }

    process->fd_table = calloc(1, sizeof(fd_table_t));
    process->fd_table->capacity = 32;
    process->fd_table->length = 0;
    process->fd_table->handles = malloc(sizeof(file_node_t*) * process->fd_table->capacity);
    memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->capacity);
    spin_unlock(&process->fd_table->lock);

    elf_t* elf = load_elf(handleElf);
    if(exec_elf(elf, 0, 0, 0)) {
        printf("Error while loading elf file.\n");
        return -1;
    }

    process->main_thread.rip = (uintptr_t)elf->entrypoint;
    process->main_thread.rsp = process->main_thread.user_stack;

    enter_user(process->main_thread.rip, process->main_thread.rsp);
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
            break;
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

process_t* get_next_process() {
    if(thread_queue->length == 0) {
        return NULL;
    }

    list_entry_t* entry = thread_queue->head;
    list_remove_by_index(thread_queue, 0);

    return (process_t*)entry->value;
}

void schedule(bool sleep) {
    if(pcb.current_process == null) return;

    if(pcb.current_process != pcb.kernel_idle_process && setjmp(&pcb.current_process->main_thread)) {
        //We are back in kernel space, resume call
        return;
    }

    __sync_and_and_fetch(&pcb.current_process->flags, ~(PROC_FLAG_ON_CPU));

    if(pcb.current_process != pcb.kernel_idle_process && !sleep) {
        schedule_process(pcb.current_process);
    }

    pcb.current_process = get_next_process();

    if(pcb.current_process == null) {
        //printf("Jumping to idle thread.\n");
        pcb.current_process = pcb.kernel_idle_process;

        set_stack_pointer(pcb.current_process->main_thread.kernel_stack);
        longjmp(&pcb.kernel_idle_process->main_thread);
    }

    //printf("Jumping to thread.\n");

    __sync_or_and_fetch(&pcb.current_process->flags, PROC_FLAG_ON_CPU);

    set_stack_pointer(pcb.current_process->main_thread.kernel_stack);
    longjmp(&pcb.current_process->main_thread);
}

void schedule_process(process_t* process) {
    __sync_and_and_fetch(&process->flags, ~(PROC_FLAG_SLEEP_INTERRUPTIBLE));

    list_insert(thread_queue, process);
}

void wait_for_object(mutex_t* mutex) {
    spin_lock(&mutex->lock);

    list_insert(mutex->waiting, get_current_process());
    __sync_or_and_fetch(&get_current_process()->flags, PROC_FLAG_SLEEP_INTERRUPTIBLE);

    spin_unlock(&mutex->lock);

    schedule(true);
}

void wakeup_waiting(list_t* queue) {
    while(queue->length > 0) {
        list_entry_t* entry = queue->head;

        if(entry != NULL && entry->value != NULL) {
            process_t* process = entry->value;

            schedule_process(process);
        }

        list_remove_by_index(queue, 0);
    }
}

void process_terminate(process_t* process, int retval) {
    if(process->id == 1) {
        printf("PANIC: Init process tried to exit!!");
        panic();
    }

    process->status = retval;

    if(process->fd_table->length > 0) {
        spin_lock(&process->fd_table->lock);

        int length = process->fd_table->length;
        int capacity = process->fd_table->capacity;
        int i = 0;
        while(length > 0) {
            if(!process->fd_table->handles[i]) {
                i++;
                continue;
            }

            free(process->fd_table->handles[i]);
            process->fd_table->handles[i] = NULL;

            i++;
            length--;

            if(i > capacity) {
                break;
            }
        }

        spin_unlock(&process->fd_table->lock);

        free(process->fd_table);
        process->fd_table = NULL;
    }


    process->flags = PROC_FLAG_FINISHED;
}

void process_thread_exit(int retval) {
    process_t* process = get_current_process();

    if(process->id == 1) {
        printf("PANIC: Init process tried to exit!!");
        panic();
    }

    process->status = retval;
    process->flags = PROC_FLAG_FINISHED;
    //Now we wait until someone cleans it up.

    schedule(true);
}

void process_exit(int retval) {
    process_t* process = get_current_process();

    if(process->id == 1) {
        printf("PANIC: Init process tried to exit!!");
        panic();
    }

    process->status = retval;

    if(process->fd_table->length > 0) {
        spin_lock(&process->fd_table->lock);

        int length = process->fd_table->length;
        int capacity = process->fd_table->capacity;
        int i = 0;
        while(length > 0) {
            if(!process->fd_table->handles[i]) {
                i++;
                continue;
            }

            free(process->fd_table->handles[i]);
            process->fd_table->handles[i] = NULL;

            i++;
            length--;

            if(i > capacity) {
                break;
            }
        }

        spin_unlock(&process->fd_table->lock);

        free(process->fd_table);
        process->fd_table = NULL;
    }


    process->flags = PROC_FLAG_FINISHED;

    //Kill other processes
    if(process_list->length > 1) {
        tree_node_t* parent = tree_find_child_root(process_tree, process);

        if(parent->children->length > 0) {
            //Kill em
            for(list_entry_t* entry = parent->children->head; entry; entry = entry->next) {
                process_t* other = (process_t*)entry->value;

                //This is a direct thread
                if(other->tgid == process->id && (process->flags & PROC_FLAG_FINISHED) == 0) {
                    process_terminate(other, retval);
                }
            }
        }
    }
    //Now we wait until someone cleans it up.

    schedule(true);
}

void sleep(long milliseconds) {
    unsigned long counter = get_counter();

    unsigned long until = counter + (milliseconds / 10) + 1;

    process_t* process = get_current_process();
    process->sleepTick = until;

    __sync_or_and_fetch(&process->flags, PROC_FLAG_SLEEP_INTERRUPTIBLE);
    __sync_and_and_fetch(&process->flags, ~(PROC_FLAG_ON_CPU));

    spin_lock(sleep_lock);
    list_insert(sleeping_queue, process);
    spin_unlock(sleep_lock);

    schedule(true);
}

void wakeup_sleeping() {
    unsigned long counter = get_counter();

    spin_lock(sleep_lock);
    if(sleeping_queue->length > 0) {
        for(list_entry_t* entry = sleeping_queue->head; entry; entry = entry->next) {
            process_t* proc = (process_t*)entry->value;

            if(proc->sleepTick <= counter) {
                list_delete(sleeping_queue, entry);

                schedule_process(proc);
            }
        }
    }
    spin_unlock(sleep_lock);
}

bool wakeup_now(process_t* proc) {
    spin_lock(sleep_lock);

    if(proc->cpu != pcb.core || proc->cpu != -1) {
        return false;
    }

    list_entry_t* entry = list_find(sleeping_queue, proc);

    if(entry != NULL) {
        proc->sleepTick = 0;
        list_delete(sleeping_queue, entry);

        schedule_process(proc);
    }

    spin_unlock(sleep_lock);

    return true;
}

int process_signal_return() {
    process_t* current = get_current_process();
    uintptr_t rsp = current->saved_registers->rsp;

    long signum = 0;
    regs_t regs;

    POP_PTR(rsp, sigset_t, get_current_process()->blocked_signals);
    POP_PTR(rsp, long, signum);
    POP_PTR(rsp, regs_t, regs);

    current->saved_registers->rsp = regs.rsp;
    current->saved_registers->rip = regs.rip;
    current->saved_registers->rax = regs.rax;
    current->saved_registers->rbx = regs.rbx;
    current->saved_registers->rcx = regs.rcx;
    current->saved_registers->rdx = regs.rdx;
    current->saved_registers->rsi = regs.rsi;
    current->saved_registers->rdi = regs.rdi;
    current->saved_registers->rbp = regs.rbp;
    current->saved_registers->r8 = regs.r8;
    current->saved_registers->r9 = regs.r9;
    current->saved_registers->r10 = regs.r9;
    current->saved_registers->r11 = regs.r9;
    current->saved_registers->r12 = regs.r9;
    current->saved_registers->r13 = regs.r9;
    current->saved_registers->r14 = regs.r9;
    current->saved_registers->r15 = regs.r9;

    current->saved_registers->rflags = regs.rflags | (1 << 9) | (1 << 21);

    return signum;
}

void process_enter_signal(regs_t* regs, int signum) {
    uintptr_t rsp = regs->rsp;

    PUSH_PTR(rsp, regs_t, *regs);
    PUSH_PTR(rsp, long, signum);
    PUSH_PTR(rsp, sigset_t, get_current_process()->blocked_signals);

    asm volatile(
            "pushq %0\n"
            "pushq %1\n"
            "pushq %2\n"
            "pushq %3\n"
            "pushq %4\n"
            "swapgs\n"
            "iretq"
            : : "g"(0x20), "g"(rsp), "g"(0x200), "g"(0x18), "m"(get_current_process()->signalHandlers[signum].handler), "D"(signum)
            );
}

file_node_t* get_cwd() {
    process_t* current = get_current_process();

    if(current == NULL) return NULL;

    if(current->cwd_file == NULL) {
        file_node_t* cwd_file = open(current->cwd, 0);

        if(cwd_file == NULL) {
            return open("/", 0);
        }

        current->cwd_file = cwd_file;
        return current->cwd_file;
    }

    return current->cwd_file;
}

char* get_cwd_name() {
    process_t* current = get_current_process();

    if(current == NULL) return "/";

    if(current->cwd_file == NULL) {
        file_node_t* cwd_file = open(current->cwd, 0);

        if(cwd_file == NULL) {
            return "/";
        }

        current->cwd_file = cwd_file;
        return current->cwd;
    }

    return current->cwd;
}