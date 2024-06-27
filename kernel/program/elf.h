//
// Created by Jannik on 19.06.2024.
//

#ifndef NIGHTOS_ELF_H
#define NIGHTOS_ELF_H

#include <stdint.h>
#include "../fs/vfs.h"

#define ELF_MAGIC 0x464C457F

#define EI_NIDENT 16

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

typedef struct
{
    unsigned char e_ident[16]; /* ELF identification */
    Elf64_Half e_type; /* Object file type */
    Elf64_Half e_machine; /* Machine type */
    Elf64_Word e_version; /* Object file version */
    Elf64_Addr e_entry; /* Entry point address */
    Elf64_Off e_phoff; /* Program header offset */
    Elf64_Off e_shoff; /* Section header offset */
    Elf64_Word e_flags; /* Processor-specific flags */
    Elf64_Half e_ehsize; /* ELF header size */
    Elf64_Half e_phentsize; /* Size of program header entry */
    Elf64_Half e_phnum; /* Number of program header entries */
    Elf64_Half e_shentsize; /* Size of section header entry */
    Elf64_Half e_shnum; /* Number of section header entries */
    Elf64_Half e_shstrndx; /* Section name string table index */
} Elf64_Ehdr;

struct elf32_program_header {
    uint32_t    type;
    uint32_t    offset;
    uint32_t    virt_addr;
    uint32_t    phys_addr;
    uint32_t    file_size;
    uint32_t    mem_size;
    uint32_t    flags;
    uint32_t    alignment;
} __attribute__((packed));

struct elf64_program_header {
    uint32_t    type;
    uint32_t    flags;
    uint64_t    offset;
    uint64_t    virt_addr;
    uint64_t    phys_addr;
    uint64_t    file_size;
    uint64_t    mem_size;
    uint64_t    alignment;
} __attribute__((packed));

typedef struct elf_file {
    Elf64_Ehdr header;

    file_handle_t* handle;
    void* entrypoint;
} elf_t;

elf_t* load_elf(file_handle_t* file);
int exec_elf(elf_t* elf_file, int argc, char* argv, char* envp);

#endif //NIGHTOS_ELF_H
