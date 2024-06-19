//
// Created by Jannik on 19.06.2024.
//
typedef int (*syscall_t)(long,long,long,long,long);

syscall_t syscall_table[256];