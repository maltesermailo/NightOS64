[BITS 64]
global reloadSegments
reloadSegments:
    push 0x08
    lea rax, [rel reloadCS]
    push rax
    retfq

reloadCS:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret