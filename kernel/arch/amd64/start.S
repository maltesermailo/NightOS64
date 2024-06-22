[BITS 32]
; Declare constants for the multiboot header.
MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

; Multiboot2
ARCHITECTURE                     equ 0
MULTIBOOT2_HEADER_MAGIC          equ 0xe85250d6
MULTIBOOT_HEADER_TAG_END         equ 0
MULTIBOOT_HEADER_TAG_FRAMEBUFFER equ 5
MULTIBOOT_HEADER_TAG_OPTIONAL    equ 1
 
section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

align 8
multiboot_header:
        dd MULTIBOOT2_HEADER_MAGIC
        dd ARCHITECTURE
        dd multiboot_header_end - multiboot_header
        dd -(MULTIBOOT2_HEADER_MAGIC + ARCHITECTURE + (multiboot_header_end - multiboot_header))

align 8
entry_address_tag_start:
        dw 3
        dw 0
        dd (entry_address_tag_end - entry_address_tag_start)
        dd _start
entry_address_tag_end:
align 8
        dw 4
        dw 0
        dw 12
        dd 3
framebuffer_tag_start:
        dw MULTIBOOT_HEADER_TAG_FRAMEBUFFER
        dw MULTIBOOT_HEADER_TAG_OPTIONAL
        dd framebuffer_tag_end - framebuffer_tag_start
        dd 85
        dd 25
        dd 0
framebuffer_tag_end:
align 8
        dw MULTIBOOT_HEADER_TAG_END
        dw 0
        dd 8
multiboot_header_end:

section .bss
align 16
stack_bottom:
resb 1048576 ; 1 MiB
global stack_top
stack_top:

section .text

GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The kernel mode code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .UserCode: equ $ - GDT64
    dw 0
    dw 0
    db 0
    db 11111010b
    db 10101010b
    db 0
    .UserData: equ $ - GDT64
    dw 0
    dw 0
    db 0
    db 11110010b
    db 11000000b
    db 0
    .TaskState: equ $ - GDT64
    dw 1                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10001001b                 ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
    dq 0                         ; Base (top)
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64                     ; Base.

global _start
_start:
	; Check for CPUID
	mov esp, stack_top
	and esp, -16

    push 0
	push ebx
	push 0
	push eax

	pushfd
	pop eax

	mov ecx, eax

	xor eax, 1 << 21

	push eax
	popfd

	pushfd
	pop eax

	push ecx
	popfd

	xor eax, ecx
	jz .NoCPUID

	; Check for Long Mode
	mov eax, 0x80000000
    cpuid                  
    cmp eax, 0x80000001    
    jb .NoLongMode  

	mov eax, 0x80000001
    cpuid                 
    test edx, 1 << 29      
    jz .NoLongMode

    mov eax, cr0
    and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
    or ax, 0x2			;set coprocessor monitoring  CR0.MP
    mov cr0, eax
    mov eax, cr4
    or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, eax
	
	; Setup PML4
    mov edi, 0x1000    ; Set the destination index to 0x1000.
    mov cr3, edi       ; Set control register 3 to the destination index.
    xor eax, eax       ; Nullify the A-register.
    mov ecx, 8192      ; Set the C-register to 4096.
    rep stosd          ; Clear the memory.
    mov edi, cr3       ; Set the destination index to control register 3.

	; Create PDPT, PDP, PT
    mov DWORD [edi], 0x2007      ; Set the uint32_t at the destination index to 0x2003.
    add edi, 0x1000              ; Add 0x1000 to the destination index.
    mov DWORD [edi], 0x3007      ; Set the uint32_t at the destination index to 0x3003.
    add edi, 0x1000              ; Add 0x1000 to the destination index.
    mov DWORD [edi], 0x00000083
    add edi, 8
    mov DWORD [edi], 0x00200083
    add edi, 8
    mov DWORD [edi], 0x00400083
    add edi, 8
    mov DWORD [edi], 0x00600083

	; Enable paging
    mov eax, cr4                 ; Set the A-register to control register 4.
    or eax, 1 << 5               ; Set the PAE-bit, which is the 6th bit (bit 5).
    mov cr4, eax                 ; Set control register 4 to the A-register.

    mov ecx, 0xC0000080          ; Set the C-register to 0xC0000080, which is the EFER MSR.
    rdmsr                        ; Read from the model-specific register.
    or eax, 1 << 8               ; Set the LM-bit which is the 9th bit (bit 8).
    wrmsr                        ; Write to the model-specific register.

    mov eax, cr0                 ; Set the A-register to control register 0.
    or eax, 1 << 31              ; Set the PG-bit, which is the 31nd bit, and the PM-bit, which is the 0th bit.
    mov cr0, eax                 ; Set control register 0 to the A-register.

	lgdt [GDT64.Pointer]
    jmp GDT64.Code:.LongMode
 
.end:

.NoCPUID:
	cli
    hlt
    jmp .hang

.NoLongMode:
	cli
    hlt
	jmp .hang

[BITS 64]
PAGE_PRESENT  equ 1 << 0
PAGE_WRITABLE equ 1 << 1
PAGE_USER     equ 1 << 2

.LongMode:
    mov rdi, 0x6000
    mov rcx, 4096 ; Clear the next 4096*8 byte
    rep stosd

    mov rdi, 0x1FF0
    mov QWORD [rdi], 0x6007
    mov rdi, 0x6000
    mov QWORD [rdi], 0x7007
    add rdi, 0x1000
    mov QWORD [rdi], 0x8007
    add rdi, 8
    mov QWORD [rdi], 0x9007
    mov rdi, 0x8000

    extern _bootstrap_end

    ; Map 4 MB after Kernel start
    mov rbx, _bootstrap_end
    or rbx, PAGE_PRESENT
    or rbx, PAGE_WRITABLE
    mov rcx, 1024                 ; Set the C-register to 1024 to map the next 4 MB.

.SetEntryLM:
    mov QWORD [rdi], rbx         ; Set the uint32_t at the destination index to the B-register.
    add rbx, 0x1000              ; Add 0x1000 to the B-register.
    add rdi, 8                   ; Add eight to the destination index.
    loop .SetEntryLM             ; Set the next entry.

    mov rdi, 0x1FF8
    mov QWORD [rdi], 0x1003

    mov ax, GDT64.Data
    mov ds, ax                    ; Set the data segment to the A-register.
    mov es, ax                    ; Set the extra segment to the A-register.
    mov fs, ax                    ; Set the F-segment to the A-register.
    mov gs, ax                    ; Set the G-segment to the A-register.
    mov ss, ax                    ; Set the stack segment to the A-register.

    pop rdi
    pop rsi

    mov rbx, 0x1000

    mov rax, 0xffffff0000000000
    jmp rax

    cli
.hang: hlt
    jmp .hang

global load_page_map0
load_page_map0:
    mov cr3, rdi
    ret
      