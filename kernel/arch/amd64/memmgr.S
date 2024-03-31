[BITS 64]
section .text
global reloadPML
reloadPML:
    mov rax, cr3
    mov cr3, rax
    ret