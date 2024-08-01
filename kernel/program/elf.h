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

#define R_X86_64_NONE             0
#define R_X86_64_64               1
#define R_X86_64_PC32             2
#define R_X86_64_GOT32            3
#define R_X86_64_PLT32            4
#define R_X86_64_COPY             5
#define R_X86_64_GLOB_DAT         6
#define R_X86_64_JUMP_SLOT        7
#define R_X86_64_RELATIVE         8
#define R_X86_64_GOTPCREL         9
#define R_X86_64_32               10
#define R_X86_64_32S              11

#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6

#define ELF64_R_SYM(i)    ((i) >> 32)
#define ELF64_R_TYPE(i)   ((i) & 0xFFFFFFFFL)
#define ELF64_R_INFO(s,t) (((s) << 32) + ((t) & 0xFFFFFFFFL))

#define SHT_NULL          0
#define SHT_PROGBITS      1
#define SHT_SYMTAB        2
#define SHT_STRTAB        3
#define SHT_RELA          4
#define SHT_HASH          5
#define SHT_DYNAMIC       6
#define SHT_NOTE          7
#define SHT_NOBITS        8
#define SHT_REL           9
#define SHT_SHLIB         10
#define SHT_DYNSYM        11
#define SHT_LOOS          0x60000000
#define SHT_HIOS          0x6FFFFFFF
#define SHT_LOPROC        0x70000000
#define SHT_HIPROC        0x7FFFFFFF

#define SHN_UNDEF    0
#define SHN_LOPROC   0xFF00
#define SHN_HIPROC   0xFF1F
#define SHN_LOOS     0xFF20
#define SHN_HIOS     0xFF3F
#define SHN_ABS      0xFFF1
#define SHN_COMMON   0xFFF2

#define SHF_WRITE         0x00000001
#define SHF_ALLOC         0x00000002
#define SHF_EXECINSTR     0x00000004
#define SHF_MASKOS        0x0F000000
#define SHF_MASKPROC      0xF0000000

typedef struct Elf64_Rel {
    Elf64_Addr  r_offset;
    Elf64_Xword r_info;
} Elf64_Rel;

typedef struct Elf64_Rela {
    Elf64_Addr   r_offset;
    Elf64_Xword  r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

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

typedef struct Elf64_Shdr {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct Elf64_Sym {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
} Elf64_Sym;


typedef struct ModuleDefinition {
    const char* name;
    int (*init)(void);
    int (*shutdown)(void);
} module_definition_t;

typedef struct Module {
    module_definition_t* definition;
    uintptr_t base;
    uint64_t size;
} module_t;
elf_t* load_elf(file_handle_t* file);
int exec_elf(elf_t* elf_file, int argc, char* argv, char* envp);

#endif //NIGHTOS_ELF_H
