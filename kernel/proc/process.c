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

#define PUSH_PTR(stack, type, value) { \
            stack -= sizeof(type);     \
            while (stack & (sizeof(type)-1)) stack--; \
            *((type*)stack) = (value);\
}

static struct process_control_block pcb; //currently the only one, we're running single core
static int id_generator = 1;
list_t* process_list;
tree_t* process_tree;
list_t* thread_queue; //Queue of threads for the scheduler

extern void longjmp(kernel_thread_t* thread);
extern int setjmp(kernel_thread_t* thread);
extern void enter_user(uintptr_t rip, uintptr_t rsp);
extern void enter_kernel(uintptr_t rip, uintptr_t rsp);
extern void enter_user_2(uintptr_t rip, uintptr_t rsp);

_Noreturn void idle() {
    while(1) {
        __asm__ volatile("sti"); //Enable interrupts while waiting.
        __asm__ volatile("hlt");
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
    process_list = list_create();
    process_tree = tree_create();
    thread_queue = list_create();
}

void process_create_task(char* path, bool is_kernel) {
    file_node_t* node = open(path, 0);

    if(node == NULL) {
        printf("Error: can't open %s\n", path);
        return;
    }

    file_handle_t* handle = create_handle(node);

    process_t* process = calloc(1, sizeof(process_t));

    process->id = 1;
    process->tgid = 0;

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

    process->fd_table = calloc(1, sizeof(fd_table_t));
    process->fd_table->capacity = 32;
    process->fd_table->length = 0;
    process->fd_table->handles = malloc(sizeof(file_node_t*) * process->fd_table->capacity);
    memset(process->fd_table->handles, 0, sizeof(file_node_t*) * process->fd_table->capacity);
    spin_unlock(&process->fd_table->lock);

    set_stack_pointer(process->main_thread.kernel_stack);

    list_insert(process_list, process);
    tree_insert_child(process_tree, NULL, process);

    unsigned long userStack = process->main_thread.user_stack;

    printf("Before 0x%x\n", userStack);

    PUSH_PTR(userStack, uintptr_t, 0); //ENVP ZERO
    PUSH_PTR(userStack, uintptr_t, 0); //ENVP
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

pid_t process_fork() {
    process_t* process = calloc(1, sizeof(process_t));
    process_t* parent = get_current_process();

    process->id = ++id_generator;
    process->parent = parent->id;
    process->tgid = 0;
    process->page_directory = calloc(1, sizeof(mm_struct_t));
    process->page_directory->process_count = 1;
    process->page_directory->page_directory = kalloc_frame();
    spin_unlock(&process->page_directory->lock);

    memmgr_clone_page_map(memmgr_get_current_pml4(), memmgr_get_from_physical(process->page_directory->page_directory));

    process->main_thread.process = process;
    process->main_thread.priority = parent->main_thread.priority;
    process->main_thread.rip = parent->main_thread.rip;
    process->main_thread.rbp = parent->main_thread.rbp;
    process->main_thread.rbx = parent->main_thread.rbx;
    process->main_thread.rdi = parent->main_thread.rdi;
    process->main_thread.r12 = parent->main_thread.r12;
    process->main_thread.r13 = parent->main_thread.r13;
    process->main_thread.r14 = parent->main_thread.r14;
    process->main_thread.r15 = parent->main_thread.r15;
    process->main_thread.user_stack = parent->main_thread.user_stack;
    process->main_thread.kernel_stack = (uintptr_t) (malloc(16384) + 16384);
    process->main_thread.rsp = parent->flags & PROC_FLAG_KERNEL ? process->main_thread.kernel_stack : process->main_thread.user_stack;

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

uintptr_t process_get_current_pml() {
    return pcb.current_page_map;
}

void process_set_current_pml(uintptr_t pml) {
    pcb.current_page_map = pml;
}

void process_free_pml(uintptr_t pml) {
    memmgr_clear_page_map(pml);
}

int execve(char* path, char* argv, char* envp) {
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

    file_handle_t* handle = create_handle(node);

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

    elf_t* elf = load_elf(handle);
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

    if(setjmp(&pcb.current_process->main_thread)) {
        //We are back in kernel space, resume call
        return;
    }

    if(!sleep) {
        schedule_process(pcb.current_process);
    }

    pcb.current_process = get_next_process();

    if(pcb.current_process == null) {
        pcb.current_process = pcb.kernel_idle_process;

        longjmp(&pcb.kernel_idle_process->main_thread);
    }

    longjmp(&pcb.current_process->main_thread);
}

void schedule_process(process_t* process) {
    list_insert(thread_queue, process);
}

void wait_for_object(mutex_t* mutex) {
    spin_lock(&mutex->lock);

    list_insert(mutex->waiting, get_current_process());
    __sync_and_and_fetch(&get_current_process()->flags, ~(PROC_FLAG_SLEEP_INTERRUPTIBLE));

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

void process_terminate(process_t* process) {

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

    if(process->tgid != process->id) {
        //Not the parent

    } else {
        //Kill other processes
        if(process_list->length > 1) {
            tree_node_t* parent = tree_find_child_root(process_tree, process);

            if(parent->children->length > 0) {
                //Kill em
                for(list_entry_t* entry = parent->children->head; entry; entry = entry->next) {
                    process_t* other = (process_t*)entry->value;

                    //This is a direct thread
                    if(other->tgid == process->id) {
                        process_terminate(other);
                    }
                }
            }
        }

        process->flags = PROC_FLAG_FINISHED;
        //Now we wait until someone cleans it up.

        schedule(true);
    }
}