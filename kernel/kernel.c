#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <xmmintrin.h>
#include "multiboot2.h"
#include "memmgr.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "timer.h"
#include "proc/process.h"
#include "pci/pci.h"
#include "../libc/include/kernel/hashtable.h"
#include "test.h"
#include "serial.h"
#include "fs/tarfs.h"
#include "fs/console.h"
#include "fs/fat.h"
#include "acpi.h"
#include "symbol.h"
#include "fs/ramfs.h"
#include "proc/message.h"
#include "../mlibc/abis/linux/fcntl.h"
#define SSFN_CONSOLEBITMAP_TRUECOLOR        /* use the special renderer for 32 bit truecolor packed pixels */
#define SSFN_CONSOLEBITMAP_CONTROL
//#define SSFN_IMPLEMENTATION
#define SSFN_realloc realloc
#define SSFN_free    free
#define SSFN_memset  memset
#define SSFN_memcmp  memcmp
#include "ssfn.h"

/* Hardware text mode color constants. */
enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
	return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color)
{
	return (uint16_t)uc | (uint16_t)color << 8;
}

size_t strlen(const char* str)
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
size_t terminal_width;
size_t terminal_height;
size_t char_width;
size_t char_height;
uint8_t terminal_color;
uint16_t* terminal_buffer;
uint8_t* back_buffer;
bool use_framebuffer = false;
ssfn_t ctx = { 0 };
ssfn_buf_t buf = { 0 };

RSDP_t* rsdp;

extern unsigned long long _physical_end;
uintptr_t kernel_end = 0;

extern char font_data_start[];
extern char font_data_end[];

#define MAX_SYMBOLS 1024  // Adjust as needed

static struct hashtable* ksymtab;
static unsigned int num_symbols = 0;

void init_kernel_symbols(void) {
    extern struct kernel_symbol __start___ksymtab[];
    extern struct kernel_symbol __stop___ksymtab[];

    ksymtab = ht_create(MAX_SYMBOLS);

    struct kernel_symbol *sym;

    for (sym = __start___ksymtab; sym < __stop___ksymtab; sym++) {
        register_symbol(sym->name, sym->value);
    }
}

int register_symbol(const char *name, unsigned long value) {
    if (num_symbols >= MAX_SYMBOLS) {
        return -1;
    }

    struct kernel_symbol* kernelSymbol = calloc(1, sizeof(struct kernel_symbol));
    kernelSymbol->name = name;
    kernelSymbol->value = value;
    ht_insert(ksymtab, name, kernelSymbol);

    num_symbols++;

    return 0;
}

struct kernel_symbol *find_symbol(const char *name) {
    return ht_lookup(ksymtab, name);
}

void terminal_initialize(int sizeX, int sizeY, int bytesPerLine)
{
	if(use_framebuffer) {
        terminal_row = 0;
        terminal_column = 0;
        terminal_color = 0;

        back_buffer = calloc(1, sizeY * bytesPerLine);

#ifdef SSFN_CONSOLEBITMAP_TRUECOLOR
        ssfn_src = (ssfn_font_t *) &font_data_start;
        ssfn_dst.ptr = (uint8_t *) back_buffer;
        ssfn_dst.p = bytesPerLine;
        ssfn_dst.fg = 0xFFFFFFFF;
        ssfn_dst.bg = 0;
        ssfn_dst.w = sizeX;
        ssfn_dst.h = sizeY;
        ssfn_dst.x = 0;
        ssfn_dst.y = 0;
#endif

#ifdef SSFN_IMPLEMENTATION
        buf = (ssfn_buf_t) {                                  /* the destination pixel buffer */
                .ptr = (uint8_t *) back_buffer,                      /* address of the buffer */
                .w = sizeX,                             /* width */
                .h = sizeY,                             /* height */
                .p = bytesPerLine,                         /* bytes per line */
                .x = 0,                                       /* pen position */
                .y = 16,
                .fg = 0xFFFFFFFF                                /* foreground color */
        };

        ssfn_load(&ctx, &font_data_start);
        ssfn_select(&ctx, SSFN_FAMILY_ANY, NULL, SSFN_STYLE_REGULAR | SSFN_STYLE_ABS_SIZE, 16);
#endif
    } else {
        terminal_row = 0;
        terminal_column = 0;
        terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_buffer = (uint16_t*)0xB8000;

        for (size_t y = 0; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                const size_t index = y * VGA_WIDTH + x;
                terminal_buffer[index] = vga_entry(' ', terminal_color);
            }
        }
    }
}

void terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    if(c == '\n')
        return;

	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void wc_memcpy(void *dst, const void *src, size_t n) {
    unsigned long long *d = (unsigned long long *)dst;
    const unsigned long long *s = (const unsigned long long *)src;
    size_t count = n / sizeof(unsigned long long);

    while (count >= 4) {
        __builtin_prefetch(s + 4, 0, 0);
        unsigned long long v1 = *s++;
        unsigned long long v2 = *s++;
        unsigned long long v3 = *s++;
        unsigned long long v4 = *s++;
        *d++ = v1;
        *d++ = v2;
        *d++ = v3;
        *d++ = v4;
        count -= 4;
    }
    while (count--) {
        *d++ = *s++;
    }
    __asm__ volatile("mfence" ::: "memory");
}

void terminal_swap() {
    if(use_framebuffer) {
#ifdef SSFN_IMPLEMENTATION
        wc_memcpy(terminal_buffer, back_buffer, buf.h * buf.p);
#endif
#ifdef SSFN_CONSOLEBITMAP_TRUECOLOR
        wc_memcpy(terminal_buffer, back_buffer, ssfn_dst.h * ssfn_dst.p);
#endif
    }
}

void terminal_rollover() {
    if(use_framebuffer) {
#ifdef SSFN_IMPLEMENTATION
        uint8_t* buffer = back_buffer;

        for (size_t y = 1; y < buf.h; y++) {
            for (size_t x = 0; x < buf.w; x++) {
                const size_t index = y * buf.h;
                const size_t new_index = (y-1) * buf.h;

                buffer = back_buffer + y * buf.p;
                uint8_t new_location = backbuffer + (y-1) * buf.p;

                memset(new_location, 0, buf.p);
                wc_memcpy(new_location, buffer, buf.p);
            }
        }

        if(buf.x + 16 > buf.h) {
            buf.x = buf.h - 16;
        }

        buffer = back_buffer + y * buf.p;
        memset(buffer, 0, buf.p);
        terminal_swap();
#endif
    } else {
        for (size_t y = 1; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                const size_t index = y * VGA_WIDTH + x;
                const size_t new_index = (y-1) * VGA_WIDTH + x;

                terminal_buffer[new_index] = terminal_buffer[index];
            }
        }

        for(int x = 0; x < VGA_WIDTH; x++) {
            const size_t index = 24 * VGA_WIDTH + x;

            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_putchar(char c)
{
    if(use_framebuffer) {
        char p[2] = { c, '\0' };
        if(c == '\b') {
            ssfn_dst.x -= char_width;

            if(ssfn_dst.x < 0) {
                ssfn_dst.x = 0;
                ssfn_dst.y -= ssfn_src->height;

                if(ssfn_dst.y < 0) {
                    ssfn_dst.y = 0;
                }
            }
            return;
        }

        if(c == '\a') {
            return;
        }

        if(c == ' ') {
            //Used to clear
            ssfn_dst.bg = 0xFF000000;
        }
#ifdef SSFN_CONSOLEBITMAP_TRUECOLOR
        int err = ssfn_putc(c);
#endif
#ifdef SSFN_IMPLEMENTATION
        int err = ssfn_render(&ctx, &buf, (const char*) &p);
#endif

#ifdef SSFN_CONSOLEBITMAP_TRUECOLOR
        if(c == ' ') {
            //Used to clear
            ssfn_dst.bg = 0;
        }
#endif

        if(err < 0) {
            serial_printf("Print error: %d", err);
        }

#ifdef SSFN_IMPLEMENTATION
        if (buf.x + 16 > buf.w && buf.y + 16 > buf.h) {
            buf.x = 0;
            terminal_rollover();
        }
#endif
    } else {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);

        if (++terminal_column == VGA_WIDTH || c == '\n') {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) {
                //Instead of moving up we a
                terminal_rollover();
                terminal_row = VGA_HEIGHT - 1;
            }
        }
    }
}

void terminal_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
    terminal_swap();
}

void terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}

void terminal_clear() {
    if(use_framebuffer) {
        memset(back_buffer, 0, buf.h * buf.p);

        terminal_swap();
    } else {
        for (size_t y = 0; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                const size_t index = y * VGA_WIDTH + x;
                terminal_buffer[index] = vga_entry(' ', terminal_color);
            }
        }
    }
}

void terminal_setcursor(int x, int y) {
    if(use_framebuffer) {
#ifdef SSFN_IMPLEMENTATION
        buf.x = (buf.w / terminal_width) * x;
        buf.y = (buf.h / terminal_height) * y;
#endif
#ifdef SSFN_CONSOLEBITMAP_TRUECOLOR
        ssfn_dst.x = (ssfn_dst.w / terminal_width) * x;
        ssfn_dst.y = (ssfn_dst.h / terminal_height) * y;
#endif
    } else {
        terminal_column = x;
        terminal_row = y;
    }
}

void terminal_resetline() {
    if(use_framebuffer) {
#ifdef SSFN_IMPLEMENTATION
        buf.x = 0;

        for(int i = 0; i < buf.p; i++) {
            //ssfn_putc(' ');
        }

        buf.x = 0;
#endif
#ifdef SSFN_CONSOLEBITMAP_TRUECOLOR
        ssfn_dst.x = 0;

        ssfn_dst.bg = 0xFF000000;
        for(int i = 0; i < ssfn_dst.p; i++) {
            ssfn_putc(' ');
        }
        ssfn_dst.bg = 0x0;

        ssfn_dst.x = 0;
#endif
    } else {
        terminal_column = 0;

        for(int x = 0; x < VGA_WIDTH; x++) {
            const size_t index = terminal_row * VGA_WIDTH + x;

            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void
itoa_k (char *buf, int base, long d)
{
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;

    /* If %d is specified and D is minus, put ‘-’ in the head. */
    if (base == 'd' && d < 0)
    {
        *p++ = '-';
        buf++;
        ud = -d;
    }
    else if (base == 'x')
        divisor = 16;

    /* Divide UD by DIVISOR until UD == 0. */
    do
    {
        int remainder = ud % divisor;

        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
    while (ud /= divisor);

    /* Terminate BUF. */
    *p = 0;

    /* Reverse BUF. */
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2)
    {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

static bool print(const char* data, size_t length) {
    const unsigned char* bytes = (const unsigned char*) data;
    for (size_t i = 0; i < length; i++)
        terminal_putchar(bytes[i]);
    return true;
}

int printf(const char* restrict format, ...) {
    va_list parameters;
    va_start(parameters, format);

    int written = 0;

    while (*format != '\0') {
        size_t maxrem = INT_MAX - written;

        if (format[0] != '%' || format[1] == '%') {
            if (format[0] == '%')
                format++;
            size_t amount = 1;
            while (format[amount] && format[amount] != '%')
                amount++;
            if (maxrem < amount) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            print(format, amount);
            serial_print(format, amount);
            format += amount;
            written += amount;
            continue;
        }

        const char* format_begun_at = format++;

        if (*format == 'c') {
            format++;
            char c = (char) va_arg(parameters, int /* char promotes to int */);
            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                terminal_swap();
                return -1;
            }
            if (!print(&c, sizeof(c)))
                return -1;
            serial_print(&c, sizeof(c));
            written++;
        } else if (*format == 's') {
            format++;
            const char* str = va_arg(parameters, const char*);
            size_t len = strlen(str);
            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                terminal_swap();
                return -1;
            }
            if (!print(str, len))
                return -1;
            serial_print(str, len);
            written += len;
        } else if(*format == 'd' || *format == 'x' || *format == 'u') {
            long i = va_arg(parameters, long);
            if(!maxrem) {
                return -1;
            }
            char buf[255];

            itoa_k(buf, *format, i);
            format++;

            if(!print(buf, strlen(buf)))
                return -1;
            serial_print(buf, strlen(buf));
            written += strlen(buf);
        } else {
            format = format_begun_at;
            size_t len = strlen(format);
            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                terminal_swap();
                return -1;
            }
            if (!print(format, len))
                return -1;
            serial_print(format, len);
            written += len;
            format += len;
        }
    }

    terminal_swap();
    va_end(parameters);
    return written;
}

void test_task() {
    printf("Hello userspace!");

    asm volatile("cli");
    asm volatile("hlt");
}

void panic() {
    printf("KERNEL PANIC");
    asm volatile("cli");

    while(1) {
        asm volatile("hlt");
    }
}

extern void syscall_init();

void kernel_main(unsigned long magic, unsigned long header)
{
	/* Initialize terminal interface */
    if(serial_init()) {
        printf("No serial.");

        return;
    }

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printf("No multiboot detected.\n");
        return;
    }

    kernel_end = (uintptr_t) &_physical_end;

    struct multiboot_tag* tag;
    unsigned size;

    struct multiboot_tag_mmap* mmap;

    size = *(unsigned *) header;

    struct multiboot_tag_framebuffer* tagfb;

    //Multiboot parsing
    for (tag = (struct multiboot_tag *) (header + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
                                         + ((tag->size + 7) & ~7)))
    {
        serial_printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
        switch (tag->type)
        {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                serial_printf ("Command line = %s\n",
                               ((struct multiboot_tag_string *) tag)->string);
                break;
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                serial_printf ("Boot loader name = %s\n",
                               ((struct multiboot_tag_string *) tag)->string);
                break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                serial_printf ("Module at 0x%x-0x%x. Command line %s\n",
                               ((struct multiboot_tag_module *) tag)->mod_start,
                               ((struct multiboot_tag_module *) tag)->mod_end,
                               ((struct multiboot_tag_module *) tag)->cmdline);
                if(((struct multiboot_tag_module *) tag)->mod_end > kernel_end) {
                    kernel_end = ((struct multiboot_tag_module *) tag)->mod_end;
                }
                break;
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                serial_printf ("mem_lower = %uKB, mem_upper = %uKB\n",
                               ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                               ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
                break;
            case MULTIBOOT_TAG_TYPE_BOOTDEV:
                serial_printf ("Boot device 0x%x,%u,%u\n",
                               ((struct multiboot_tag_bootdev *) tag)->biosdev,
                               ((struct multiboot_tag_bootdev *) tag)->slice,
                               ((struct multiboot_tag_bootdev *) tag)->part);
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
            {
                serial_printf ("mmap\n");

                mmap = (struct multiboot_tag_mmap *) tag;
            }
                break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
            {
                multiboot_uint32_t color;
                unsigned i;
                tagfb
                        = (struct multiboot_tag_framebuffer *) tag;
                void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;

                if(tagfb->common.framebuffer_type == 2) {
                    terminal_initialize(80, 25, 0);
                    break;
                }

                use_framebuffer = true;
            }
                break;
            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
            {
                struct multiboot_tag_old_acpi* acpi = (struct multiboot_tag_old_acpi*) tag;
                rsdp = (RSDP_t *) &acpi->rsdp;
            }
        }
    }

    int terminalWidth = 0;
    int terminalHeight = 0;

    //Setup GDT and TSS
    gdt_install();

    //Set pml for memmgr
    process_set_current_pml(0x1000);

    //Setup Memory Management
    memmgr_init(mmap, kernel_end);
    idt_install();

    if(use_framebuffer) {
        memmgr_map_mmio((unsigned long) tagfb->common.framebuffer_addr, tagfb->common.framebuffer_height * tagfb->common.framebuffer_pitch, FLAG_WC, false);
        terminal_buffer = (uint16_t *) memmgr_get_mmio((unsigned long) tagfb->common.framebuffer_addr);

        terminal_initialize(tagfb->common.framebuffer_width, tagfb->common.framebuffer_height, tagfb->common.framebuffer_pitch); //Re-initialize the terminal

#ifdef SSFN_CONSOLEBITMAP_TRUECOLOR
        printf("/");
        char_width = ssfn_dst.x;
        printf("\n");
        char_height = ssfn_dst.y;
#endif

        terminalWidth = tagfb->common.framebuffer_width / 16;
        terminalHeight = tagfb->common.framebuffer_height / 16;

        memset(back_buffer, 0, tagfb->common.framebuffer_height * tagfb->common.framebuffer_pitch);
        terminal_swap();
    }

    serial_printf("Colonel version 0.0.0-5 starting up...\n");
    printf("Colonel version 0.0.0-5 starting up...\n");

    //Setup interrupts
    pic_setup();
    irq_install();
    ps2_init();
    timer_init();
    init_kernel_symbols();

    alloc_register_object_size(sizeof(list_entry_t));
    alloc_register_object_size(sizeof(list_t));
    alloc_register_object_size(sizeof(process_t));
    alloc_register_object_size(sizeof(message_t));

    //Setup filesystem and modules
    vfs_install();

    mkdir_vfs("/dev");

    for (tag = (struct multiboot_tag *) (header + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
                                         + ((tag->size + 7) & ~7)))
    {
        //printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
        switch (tag->type)
        {
            case MULTIBOOT_TAG_TYPE_MODULE:
                printf ("Module at 0x%x-0x%x. Command line %s\n",
                        ((struct multiboot_tag_module *) tag)->mod_start,
                        ((struct multiboot_tag_module *) tag)->mod_end,
                        ((struct multiboot_tag_module *) tag)->cmdline);

                if(strcmp(((struct multiboot_tag_module *) tag)->cmdline, "tarfs") == 0) {
                    for(int i = 0; i < ((struct multiboot_tag_module *) tag)->mod_end - ((struct multiboot_tag_module *) tag)->mod_start; i += 4096) {
                        memmgr_phys_mark_page(ADDRESS_TO_PAGE(((uintptr_t) ((struct multiboot_tag_module *) tag)->mod_start) + i));
                    }
                    tarfs_init("/", memmgr_get_from_physical((uintptr_t) ((struct multiboot_tag_module *) tag)->mod_start), ((struct multiboot_tag_module *) tag)->mod_end - ((struct multiboot_tag_module *) tag)->mod_start);
                }
                break;
        }
    }

    //Init pci
    pci_init(rsdp);

	//terminal_writestring("Hello Kernel\n");

    /*if(info->mods_count > 0) {
        multiboot_module_t* module = info->mods_addr;

        if(strcmp(module->cmdline, "tarfs") == 0) {
            tarfs_init("/nightos/", module->mod_start, module->mod_end);
        }
    }*/
    console_init(terminalWidth, terminalHeight);
    syscall_init();

    //Load filesystem at hd0
    mount_directly("/mnt", fat_mount("/dev/hd0", "/mnt"));
    ramfs_init("/tmp");

    printf("Performing list test now...\n");
    //list_test();
    printf("Performing tree test now...\n");
    //tree_test();
    printf("Performing VFS test now...\n");
    vfs_test();
    printf("Performing FAT test now...\n");
    fat_test();

    kmalloc_test();

    //Try opening console
    file_node_t* console0 = open("/dev/tty", 0);
    file_handle_t* hConsole = create_handle(console0);
    hConsole->mode = O_DIRECT;
    write(hConsole, "test", strlen("test")+1);

    process_init();
    process_create_idle();

    __asm__ volatile ("sti"); // set the interrupt flag

    process_create_task("/usr/bin/bash", false);

    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}

EXPORT_SYMBOL(terminal_write);
EXPORT_SYMBOL(panic);