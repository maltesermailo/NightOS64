//
// Created by Jannik on 21.06.2024.
//
#include <syscall.h>
#include <string.h>

#ifdef IDE
#include "../include/syscall.h"
#endif

void setup(void) {
    const char console[] = "/dev/console0\0";

    long fd = syscall_wrapper(SYS_OPEN, &console, 0);

    if(fd != 0) {
        __asm__ volatile("UD2"); //Trigger fault
    }

    const char setupString[] = "Setup c runtime.\0";
    syscall_wrapper(SYS_WRITE, fd, &setupString, strlen(setupString));
}