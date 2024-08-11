project('nightos', 'c',
        version : '0.1',
        default_options : ['c_std=gnu11', 'buildtype=debugoptimized'])

# Define variables
sysroot = meson.current_source_dir() / 'sysroot'
archdir = 'kernel/arch/amd64'

# Find NASM
nasm = find_program('nasm', required : true)

# Create a custom NASM compiler
nasm_gen = generator(nasm,
                     output : '@BASENAME@.o',
                     arguments : ['-f', 'elf64', '-g', '-F', 'dwarf', '@INPUT@', '-o', '@OUTPUT@'])

# Include architecture-specific configuration
subdir(archdir)

# Compiler and linker flags
add_project_arguments(
    '-ffreestanding',
    '-mcmodel=large',
    '-mno-red-zone',
    '-mno-mmx',
    '-mno-sse',
    '-mno-sse2',
    '-g',
    '--sysroot=' + sysroot,
    kernel_arch_cflags,
    language : 'c'
)

add_project_link_arguments(
    '-ffreestanding',
    '-O2',
    '-nostdlib',
    '-z', 'max-page-size=0x1000',
    '-no-pie',
    kernel_arch_ldflags,
    language : 'c'
)

# Define sources
kernel_sources = [
    'kernel/kernel.c',
    'kernel/alloc/liballoc.c',
    'kernel/proc/process.c',
    'kernel/pci/pci.c',
    'kernel/test.c',
    'kernel/serial.c',
    'kernel/fs/vfs.c',
    'kernel/fs/tarfs.c',
    'kernel/fs/console.c',
    'kernel/fs/fat.c',
    'kernel/fs/ramfs.c',
    'kernel/sys/syscall.c',
    'kernel/proc/ipc.c',
    'kernel/program/elf.c',
    'kernel/pci/ahci.c',
    'kernel/sys/mutex.c',
    'kernel/fs/pty.c',
]

# Add architecture-specific objects
kernel_sources += kernel_arch_objs

# Process NASM sources
# Compile start.S and boot.S
start_obj = custom_target('start.o',
                          input : meson.current_source_dir() / archdir / 'start.S',
                          output : 'start.o',
                          command : [nasm, '-f', 'elf64', '-g', '-F', 'dwarf', '@INPUT@', '-o', '@OUTPUT@'])

boot_obj = custom_target('boot.o',
                         input : meson.current_source_dir() / archdir / 'boot.S',
                         output : 'boot.o',
                         command : [nasm, '-f', 'elf64', '-g', '-F', 'dwarf', '@INPUT@', '-o', '@OUTPUT@'])
nasm_objs = nasm_gen.process(kernel_arch_nasm_sources)

gen_linker_script = custom_target('gen_linker_script',
                                  input : archdir + '/linker.ld',
                                  output : 'gen_linker.ld',
                                  command : ['sed', '-e', 's|kernel/arch/amd64/start.S.o|start.o|g',
                                             '-e', 's|kernel/arch/amd64/boot.S.o|boot.o|g',
                                             '@INPUT@'],
                                  capture : true
)

# Define dependencies
libgcc = meson.get_compiler('c').find_library('gcc', required : true)
libk = meson.get_compiler('c').find_library('k', required : true)

# Define the kernel executable
kernel = executable('nightos.kernel',
                    kernel_sources,
                    objects : [start_obj, boot_obj] + nasm_objs,
                    link_args : ['-T', gen_linker_script.full_path(),
                                 '-Wl,-Map=nightos.kernel.map',
                                 '-Wl,--start-group',
                                 '-lgcc', '-lk',
                                 '-Wl,--end-group'],
                    link_depends : gen_linker_script,
                    dependencies : [libgcc, libk] + kernel_arch_libs,
                    install : true,
                    install_dir : meson.current_source_dir() / 'isodir/boot'
)

# Create ISO
iso_target = custom_target('nightos.iso',
                           output : 'nightos.iso',
                           command : [
                               'sh', '-c',
                               'cd ' + meson.current_source_dir() + ' && ' +
                               'mkdir -p isodir/boot/grub/ && ' +
                               'cp grub.cfg isodir/boot/grub/grub.cfg && ' +
                               'tar -cvf rootfs.tar initd test.txt && ' +
                               'cp rootfs.tar isodir/boot/ && ' +
                               'grub-mkrescue -o ' + meson.current_build_dir() + '/@OUTPUT@ isodir && ' +
                               'rm -rf isodir rootfs.tar'
                           ],
                           depends : kernel,
                           build_by_default : true
)

# Make the ISO a default target
default_target = custom_target('default',
                               output : 'default',
                               command : ['touch', '@OUTPUT@'],
                               depends : iso_target
)