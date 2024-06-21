#include <stdio.h>
#include <syscall.h>

#if defined(__is_libk)
#include "../../kernel/terminal.h"
#endif

int putchar(int ic) {
#if defined(__is_libk)
	char c = (char) ic;
	terminal_write(&c, sizeof(c));
#else
    char c = (char) ic;
	syscall_wrapper(SYS_WRITE, &c, 1);
#endif
	return ic;
}
