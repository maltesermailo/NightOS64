# OS Build File
CFLAGS := -mno-avx -mno-sse -mno-sse2 -g -fno-PIC -fno-PIE
LDFLAGS :=-O2 -static -no-pie
NASM = nasm
BUILDDIR=build
CC=x86_64-nightos-gcc
LIBDIR:=/usr/local/lib/

OBJS=\
init.o \

LINK_LIST=\
$(LDFLAGS) \
$(OBJS) \
$(LIBS) \

.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S

all: init

init: $(OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(CFLAGS) $(LINK_LIST)

%.c.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

%.S.o: %.S
	$(NASM) -f elf64 -g -F dwarf $< -o $@

clean:
	rm -f init
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-app

install-app: init
	cp init ../initd

-include $(OBJS:.o=.d)