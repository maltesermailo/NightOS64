# OS Build File
CFLAGS ?=-ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -g
LDFLAGS ?=-ffreestanding -O2 -nostdlib -z max-page-size=0x1000
NASM = nasm
BUILDDIR=build
CC=x86_64-elf-gcc

ARCHDIR=kernel/arch/amd64
LIBS:=$(LIBS) -nostdlib -lgcc

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
kernel/kernel.o \
kernel/alloc/liballoc.o \

OBJS=\
$(KERNEL_OBJS) \

LINK_LIST=\
$(LDFLAGS) \
$(KERNEL_OBJS) \
$(LIBS) \

.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S

all: nightos.kernel

nightos.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	grub-file --is-x86-multiboot nightos.kernel

$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

%.c.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

%.S.o: %.S
	$(NASM) -f elf64 -g -F dwarf $< -o $@

clean:
	rm -f nightos.kernel
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: nightos.kernel
	mkdir -p isodir/boot/grub/
	cp nightos.kernel isodir/boot/nightos.kernel
	cp grub.cfg isodir/boot/grub/grub.cfg

build: install-kernel nightos.kernel
	grub-mkrescue -o myos.iso isodir

-include $(OBJS:.o=.d)