[BITS 64]

global setjmp
global longjmp
global enter_user
global enter_user_2
global enter_kernel
global fork_exit
setjmp:
    ; We get a structure containing all registers as a pointer, therefore we use that pointer on rdi
    ; 0 = stack pointer
    ; 8 = base pointer and so on (lookup process.h)
    lea rax, [rsp+8]
    mov [RDI], rax; Move stack pointer into structure(before this call)
    mov [RDI+8], rbp; Move base pointer into structure
    mov rax, [rsp]
    mov [RDI+16], rax; Save Instruction Pointer(as return address)
    mov qword [RDI+24], 0

    mov [RDI+32], rbx ; RBX
    mov [RDI+40], r12 ; R12
    mov [RDI+48], r13 ; R13
    mov [RDI+56], r14; R14
    mov [RDI+64], r15 ; R15
    xor rax, rax ; Cleanup rax register
    ret ; Return to switch_task

longjmp:
    mov rsp, [RDI]    ; Load stack
    mov rbp, [RDI+8]  ; Load base pointer

    mov rbx, [RDI+32] ; RBX
    mov r12, [RDI+40] ; R12
    mov r13, [RDI+48] ; R13
    mov r14, [RDI+56] ; R14
    mov r15, [RDI+64] ; R15
    mov rax, 1

    jmp qword [RDI+16] ; Jump to process

fork_exit:
%macro swapgs_if_necessary 1
	cmp QWORD [rsp+24], 0x8
	je swapgs_end_%1
	swapgs
swapgs_end_%1:
%endmacro
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    swapgs_if_necessary end

    add rsp, 16

    iretq

enter_user:
    ; RIP passed in RDI
    ; RSP passed in RSI
    mov rax, 0x20 | 0x03   ; Move data selector for user space
    mov ds, rax
    mov es, rax
    mov gs, rax
    mov fs, rax

    mov rax, rsi
    push qword 0x20 | 0x03 ; Push stack segment
    push rax               ; Push stack pointer
    push 0x200             ; Push EFLAGS
    push qword 0x18 | 0x03 ; Push code selector
    push qword rdi         ; Push RIP
    iretq                  ; Return to userspace

enter_kernel:
    ; Like enter_user, simple jump to kernel code
    mov rsp, rsi
    mov rax, rdi

    jmp rdi


enter_user_2:
    mov rcx, 0xc0000082 ; Clear LSTAR, needs to be set to syscall handler later
    wrmsr
    mov rcx, 0xc0000080 ; Enable System Call Extensions
    rdmsr
    or eax, 1
    wrmsr

    mov rcx, 0xc0000081 ; Set segment bases
    rdmsr
    mov edx, 0x00180008
    wrmsr

    mov rcx, rdi ; Set RIP
    mov r11, 0x200 ; Restore EFLAGS
    mov rsp, rsi ; Set stack
    o64 sysret ; Syscall into user mode
