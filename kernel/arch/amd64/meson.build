# Architecture-specific Meson build file (kernel/arch/amd64/meson.build)

# Define architecture-specific objects
kernel_arch_objs = [
    'memmgr.c',
    'alloc.c',
    'gdt.c',
    'idt.c',
    'pic.c',
    'hid/ps2.c',
    'pit.c',
]

# Copy the font file to the build directory
font_file = 'unifont.sfn'
fs = import('fs')
font_data = fs.copyfile(font_file)

# Convert the list of source files to full paths
kernel_arch_objs = files(kernel_arch_objs)

kernel_arch_nasm_sources = [
    'context_switch.S',
    'idt.S',
    'gdt.S',
    'memmgr.S',
    'font.S',
]

kernel_arch_nasm_sources = files(kernel_arch_nasm_sources)

# Architecture-specific compiler flags (if any)
kernel_arch_cflags = []

# Architecture-specific linker flags (if any)
kernel_arch_ldflags = []

# Architecture-specific libraries (if any)
kernel_arch_libs = []

