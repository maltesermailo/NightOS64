#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "multiboot2.h"
#include "memmgr.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "timer.h"
#include "proc/process.h"
#include "pci/pci.h"
#include "../libc/include/kernel/list.h"
#include "test.h"
#include "serial.h"
#include "fs/tarfs.h"
#include "fs/console.h"
#include "fs/fat.h"
#include "acpi.h"

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
uint8_t terminal_color;
uint16_t* terminal_buffer;
RSDP_t* rsdp;
extern unsigned long long _physical_end;
uintptr_t kernel_end = 0;


void terminal_initialize(void)
{
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

void terminal_rollover() {
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

void terminal_putchar(char c)
{
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

void terminal_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}

void terminal_resetline() {
    terminal_column = 0;

    for(int x = 0; x < VGA_WIDTH; x++) {
        const size_t index = terminal_row * VGA_WIDTH + x;

        terminal_buffer[index] = vga_entry(' ', terminal_color);
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
            if (!print(format, amount))
                return -1;
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
                return -1;
            }
            if (!print(&c, sizeof(c)))
                return -1;
            written++;
        } else if (*format == 's') {
            format++;
            const char* str = va_arg(parameters, const char*);
            size_t len = strlen(str);
            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(str, len))
                return -1;
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
            written += strlen(buf);
        } else {
            format = format_begun_at;
            size_t len = strlen(format);
            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(format, len))
                return -1;
            written += len;
            format += len;
        }
    }

    va_end(parameters);
    return written;
}

char commandline[64];

void key_event(key_event_t* event) {
    if(event->keyCode == 0x8) {
        for(int i = 0; i < 64; i++) {
            if(!commandline[i] && event->isDown) {
                commandline[i-1] = 0;
                return;
            }
        }
        return;
    }

    for(int i = 0; i < 64; i++) {
        if(!commandline[i] && event->isDown) {
            commandline[i] = event->keyCode;
            return;
        }
    }
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

void kernel_main(unsigned long magic, unsigned long header)
{
	/* Initialize terminal interface */
	terminal_initialize();
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

    serial_printf("Colonel version 0.0.0 starting up...\n");
    printf("Colonel version 0.0.0-2 starting up...\n");

    //Multiboot parsing
    for (tag = (struct multiboot_tag *) (header + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
                                         + ((tag->size + 7) & ~7)))
    {
        printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
        switch (tag->type)
        {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                printf ("Command line = %s\n",
                        ((struct multiboot_tag_string *) tag)->string);
                break;
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                printf ("Boot loader name = %s\n",
                        ((struct multiboot_tag_string *) tag)->string);
                break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                printf ("Module at 0x%x-0x%x. Command line %s\n",
                        ((struct multiboot_tag_module *) tag)->mod_start,
                        ((struct multiboot_tag_module *) tag)->mod_end,
                        ((struct multiboot_tag_module *) tag)->cmdline);
                if(((struct multiboot_tag_module *) tag)->mod_end > kernel_end) {
                    kernel_end = ((struct multiboot_tag_module *) tag)->mod_end;
                }
                break;
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                printf ("mem_lower = %uKB, mem_upper = %uKB\n",
                        ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                        ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
                break;
            case MULTIBOOT_TAG_TYPE_BOOTDEV:
                printf ("Boot device 0x%x,%u,%u\n",
                        ((struct multiboot_tag_bootdev *) tag)->biosdev,
                        ((struct multiboot_tag_bootdev *) tag)->slice,
                        ((struct multiboot_tag_bootdev *) tag)->part);
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
            {
                printf ("mmap\n");

                mmap = tag;
            }
                break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
            {
                multiboot_uint32_t color;
                unsigned i;
                struct multiboot_tag_framebuffer *tagfb
                        = (struct multiboot_tag_framebuffer *) tag;
                void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;
            }
            break;
            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
            {
                struct multiboot_tag_old_acpi* acpi = (struct multiboot_tag_old_acpi*) tag;
                rsdp = (RSDP_t *) &acpi->rsdp;
            }
        }
    }

    //Setup GDT and TSS
    gdt_install();

    //Set pml for memmgr
    process_set_current_pml(0x1000);

    //Setup Memory Management
    memmgr_init(mmap, kernel_end);

    //Setup interrupts
    idt_install();
    pic_setup();
    irq_install();
    ps2_init();
    timer_init();

    __asm__ volatile ("sti"); // set the interrupt flag

    //Setup filesystem and modules
    vfs_install();

    mkdir_vfs("/dev");
    mkdir_vfs("/test");

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
                    tarfs_init("/", (void *) ((struct multiboot_tag_module *) tag)->mod_start, ((struct multiboot_tag_module *) tag)->mod_end - ((struct multiboot_tag_module *) tag)->mod_start);
                }
                break;
        }
    }

    //Init pci
    pci_init(rsdp);

	//terminal_writestring("Hello Kernel\n");

    for(int i = 0; i < 64; i++) {
        commandline[i] = 0;
    }

    registerKeyEventHandler(key_event);

    /*if(info->mods_count > 0) {
        multiboot_module_t* module = info->mods_addr;

        if(strcmp(module->cmdline, "tarfs") == 0) {
            tarfs_init("/nightos/", module->mod_start, module->mod_end);
        }
    }*/
    console_init();

    //Load filesystem at hd0
    mount_directly("/fatfs", fat_mount("/dev/hd0", "/fatfs"));

    printf("Performing list test now...\n");
    //list_test();
    printf("Performing tree test now...\n");
    //tree_test();
    printf("Performing VFS test now...\n");
    vfs_test();
    printf("Performing FAT test now...\n");
    fat_test();

    //Try opening console
    file_node_t* console0 = open("/dev/console0", 0);
    file_handle_t* hConsole = create_handle(console0);
    write(hConsole, "test", strlen("test")+1);

    process_init();
    process_create_idle();
    process_create_task("/initd", false);

    __asm__ volatile("cli");
    __asm__ volatile("hlt");

    //process_create_task(&test_task);

    /*while(1) {
        terminal_resetline();
        printf("> ");

        for(int i = 0; i < 64; i++) {
            if(commandline[i]) {
                if(commandline[i] == '\n') {
                    //Execute command
                    if(commandline[0] == 'o' && commandline[1] == 'w' && commandline[2] == 'o') {
                        //Flip all memory bits
                        printf("\n");
                        printf("OwO");

                        for (size_t y = 0; y < VGA_HEIGHT; y++) {
                            for (size_t x = 0; x < VGA_WIDTH; x++) {
                                const size_t index = y * VGA_WIDTH + x;
                                terminal_buffer[index] = ~(terminal_buffer[index]);
                            }
                        }

                        asm volatile("cli");
                        asm volatile("hlt");
                    }
                }
                printf("%c", commandline[i]);
            }
        }
    }*/
}