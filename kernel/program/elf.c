//
// Created by Jannik on 25.06.2024.
//
#include "elf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../memmgr.h"
#include "../symbol.h"
#include "../../libc/include/kernel/hashtable.h"

struct hashtable* modules;

//TODO: Rework to use error codes
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

int module_elf(elf_t* elf_file) {
    if(elf_file->header.e_type != 1) {
        printf("No module type\n");

        return -1;
    }

    void* module_space = malloc(elf_file->handle->fileNode->size);
    void* moduleDefinition = null;

    size_t size = elf_file->handle->fileNode->size;

    read(elf_file->handle, module_space, elf_file->handle->fileNode->size);

    //First: load no bits sections
    for(int i = 0; i < elf_file->header.e_shnum; i++) {
        Elf64_Shdr* shdr = module_space + (elf_file->header.e_shoff + i * elf_file->header.e_shentsize);

        if(shdr->sh_type == SHT_NOBITS) {
            shdr->sh_addr = (Elf64_Addr) malloc(shdr->sh_size);
            size += shdr->sh_size;
            memset((void *) shdr->sh_addr, 0, shdr->sh_size);
        } else {
            shdr->sh_addr = (Elf64_Addr)(module_space + shdr->sh_offset);
        }
    }

    //Second: Resolve symbols
    for(int i = 0; i < elf_file->header.e_shnum; i++) {
        Elf64_Shdr* shdr = module_space + (elf_file->header.e_shoff + i * elf_file->header.e_shentsize);

        if(shdr->sh_type != SHT_SYMTAB) {
            continue;
        }

        Elf64_Shdr* strtab = module_space + (elf_file->header.e_shoff + shdr->sh_link * elf_file->header.e_shentsize);
        char* stringTable = (char*)strtab->sh_addr;

        Elf64_Sym* symbolTable = (Elf64_Sym*)shdr->sh_addr;

        uint32_t symCount = shdr->sh_size / sizeof(Elf64_Sym);

        for(uint32_t sym = 0; sym < symCount; sym++) {
            if(symbolTable[sym].st_shndx > 0) {
                Elf64_Shdr* other = (Elf64_Shdr*)module_space + (elf_file->header.e_shoff + symbolTable[sym].st_shndx * elf_file->header.e_shentsize);
                symbolTable[sym].st_value = symbolTable[sym].st_value + other->sh_addr;
            } else if(symbolTable[sym].st_shndx == SHN_UNDEF) {
                symbolTable[sym].st_value = (uintptr_t) find_symbol(stringTable + symbolTable[sym].st_name);
            } else {
                //Don't know
                printf("Tried to resolve unknown symbol: %s\n", stringTable + symbolTable[sym].st_name);
            }

            if (symbolTable[sym].st_name && !strcmp(stringTable + symbolTable[sym].st_name, "module")) {
                moduleDefinition = (void*)symbolTable[sym].st_value;
            }
        }
    }

    if(!moduleDefinition) {
        free(module_space);
        return -1;
    }

    //Step 3: apply relocations
    for(int i = 0; i < elf_file->header.e_shnum; i++) {
        Elf64_Shdr* shdr = module_space + (elf_file->header.e_shoff + i * elf_file->header.e_shentsize);

        if(shdr->sh_type != SHT_RELA) {
            continue;
        }

        Elf64_Rela* relocationTable = (Elf64_Rela*)shdr->sh_addr;
        Elf64_Shdr* section = (Elf64_Shdr*) module_space + (elf_file->header.e_shoff + shdr->sh_info * elf_file->header.e_shentsize);
        Elf64_Shdr* symbolHeader = (Elf64_Shdr*) module_space + (elf_file->header.e_shoff + shdr->sh_link * elf_file->header.e_shentsize);
        Elf64_Sym* symbolTable = (Elf64_Sym*) symbolHeader->sh_addr;

        uint32_t relaCount = shdr->sh_size / sizeof(Elf64_Sym);

        for(uint32_t relocation = 0; relocation < relaCount; relocation++) {
            uintptr_t target = section->sh_addr + relocationTable[relocation].r_offset;

            switch(ELF64_R_TYPE(relocationTable[relocation].r_info)) {
                case R_X86_64_32: {
                    *((uint32_t*)target) = symbolTable[ELF64_R_SYM(relocationTable[relocation].r_info)].st_value + relocationTable[relocation].r_addend;
                    break;
                }
                case R_X86_64_64: {
                    *((uint64_t*)target) = symbolTable[ELF64_R_SYM(relocationTable[relocation].r_info)].st_value + relocationTable[relocation].r_addend;
                    break;
                }
                case R_X86_64_PC32: {
                    *((uint32_t*)target) = symbolTable[ELF64_R_SYM(relocationTable[relocation].r_info)].st_value + relocationTable[relocation].r_addend - target;
                    break;
                }
                default:
                    printf("Module: unknown relocation %d\n", ELF64_R_TYPE(relocationTable[relocation].r_info));
                    break;
            }
        }
    }

    module_t* module = calloc(1, sizeof(module_t));
    module->definition = moduleDefinition;
    module->base = (uintptr_t) module_space;
    module->size = size;

    ht_insert(modules, module->definition->name, module);

    return module->definition->init();
}