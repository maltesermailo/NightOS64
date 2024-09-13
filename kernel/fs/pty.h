// pty.h

#ifndef NIGHTOS_PTY_H
#define NIGHTOS_PTY_H

#include <stdint.h>
#include <stddef.h>
#include "../proc/process.h"
#include "vfs.h"
#include "../../libc/include/kernel/ring_buffer.h"
#include "../../mlibc/abis/linux/termios.h"

#define PTY_BUFFER_SIZE 1024
#define MAX_PTY_PAIRS 16

#define TCGETS		0x5401
#define TCSETS		0x5402
#define TCSETSW		0x5403
#define TCSETSF		0x5404
#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSETAW		0x5407
#define TCSETAF		0x5408
#define TCSBRK		0x5409
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TIOCEXCL	0x540C
#define TIOCNXCL	0x540D
#define TIOCSCTTY	0x540E
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCOUTQ	0x5411
#define TIOCSTI		0x5412
#define TIOCGWINSZ	0x5413
#define TIOCSWINSZ	0x5414

#define TISTTY 0x9000

struct pty_data {
    circular_buffer_t *input_buffer;
    circular_buffer_t *output_buffer;
    struct termios *term_settings;

    pid_t session_leader;

    int index;
    bool raw;
    bool console;
};

struct pty_pair {
    file_node_t master;
    file_node_t slave;

    mutex_t wait_queue_write; //For canonical mode
    mutex_t wait_queue_read; //For canonical mode

    struct pty_data *data;
};

// Function declarations
int pty_init(void);
int pty_create_pair(int pty_index);
file_node_t* pty_get_master(int pty_index);
file_node_t* pty_get_slave(int pty_index);
int pty_write_to_input(int pty_index, const char* buffer, size_t size);
int pty_write_char_to_input(int pty_index, char c);

// File operations
int pty_slave_read(file_node_t *node, char *buffer, size_t size, size_t offset);
int pty_master_read(file_node_t *node, char *buffer, size_t size, size_t offset);
int pty_write(file_node_t *node, char *buffer, size_t size, size_t offset);
int pty_master_write(file_node_t *node, char *buffer, size_t size, size_t offset);
void pty_open(file_node_t *node, int mode);
void pty_close(file_node_t *node);
int pty_ioctl(file_node_t *node, unsigned long request, void *args);
int pty_poll(file_node_t* node, int requested);

#endif // NIGHTOS_PTY_H