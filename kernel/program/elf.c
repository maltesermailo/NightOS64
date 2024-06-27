//
// Created by Jannik on 25.06.2024.
//
#include "elf.h"
#include <stdlib.h>
#include <stdio.h>
#include "../memmgr.h"

elf_t* load_elf(file_handle_t* handle) {
    elf_t* elf = calloc(1, sizeof(elf_t));
    elf->handle = handle;

    read(handle, (char*)&elf->header, sizeof(Elf64_Ehdr));

    if(!(elf->header.e_ident[0] == 0x7F && elf->header.e_ident[1] == 'E' && elf->header.e_ident[2] == 'L' && elf->header.e_ident[3] == 'F')) {
        printf("WARNING: No elf file provided.\n");
        return NULL;
    }

    if(elf->header.e_ident[4] != 2) {
        printf("WARNING: No 64-bit executable.\n");
        return NULL;
    }
    return elf;
}

int exec_elf(elf_t* elf_file, int argc, char* argv, char* envp) {
   for(int i = 0; i < elf_file->header.e_phnum; i++) {
       struct elf64_program_header programHeader;

       elf_file->handle->offset = elf_file->header.e_phoff + i * elf_file->header.e_phentsize;
       read(elf_file->handle, (char*)&programHeader, sizeof(struct elf64_program_header));

       if(programHeader.type == 1) {
           for(uintptr_t ptr = programHeader.virt_addr; ptr < programHeader.virt_addr + programHeader.mem_size; ptr += 0x1000) {
               mmap((void*)ptr, 0x1000, false);
           }

           elf_file->handle->offset = programHeader.offset;
           read(elf_file->handle, (void*)programHeader.virt_addr, programHeader.file_size);

           memset((void*)programHeader.virt_addr + programHeader.file_size, 0, programHeader.mem_size - programHeader.file_size);
       }
   }

   elf_file->entrypoint = (void*)elf_file->header.e_entry;

   return 0;
}