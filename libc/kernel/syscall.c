//
// Created by Jannik on 21.06.2024.
//
#include <syscall.h>
#include <stdarg.h>
long syscall_wrapper(long syscall_number, ...) {
    va_list args;
    va_start(args, syscall_number);

    // Syscall arguments
    long arg1 = va_arg(args, long);
    long arg2 = va_arg(args, long);
    long arg3 = va_arg(args, long);
    long arg4 = va_arg(args, long);
    long arg5 = va_arg(args, long);
    long arg6 = va_arg(args, long);

    va_end(args);

    long ret;
    __asm__ volatile (
            "movq %1, %%rax;\n"  // Syscall number into rax
            "movq %2, %%rdi;\n"  // 1st argument into rdi
            "movq %3, %%rsi;\n"  // 2nd argument into rsi
            "movq %4, %%rdx;\n"  // 3rd argument into rdx
            "movq %5, %%r10;\n"  // 4th argument into r10 (not rcx)
            "movq %6, %%r8;\n"   // 5th argument into r8
            "movq %7, %%r9;\n"   // 6th argument into r9
            "int $0x80;\n"         // Invoke syscall
            "movq %%rax, %0;\n"  // Return value in rax
            : "=r" (ret)
            : "g" (syscall_number), "g" (arg1), "g" (arg2), "g" (arg3), "g" (arg4), "g" (arg5), "g" (arg6)
            : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rcx", "r11", "memory"
            );

    // Set errno if there's an error
    if (ret < 0) {
        //errno = -ret;
        ret = -1;
    }

    return ret;
}
