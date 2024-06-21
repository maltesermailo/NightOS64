//
// Created by Jannik on 21.06.2024.
//

#ifndef NIGHTOS_SYSCALL_H
#define NIGHTOS_SYSCALL_H
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2

long syscall_wrapper(long syscall_number, ...);

#endif //NIGHTOS_SYSCALL_H
