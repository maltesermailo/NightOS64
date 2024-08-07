// pty.h

#ifndef NIGHTOS_PTY_H
#define NIGHTOS_PTY_H

#include <stdint.h>
#include <stddef.h>
#include "../proc/process.h"
#include "vfs.h"
#include "../../libc/include/kernel/ring_buffer.h"

#define PTY_BUFFER_SIZE 1024
#define MAX_PTY_PAIRS 16
#define ECHO 1
#define ICANON 2

struct termios {
    int c_lflag;
};  // Forward declaration, you'll need to define this elsewhere

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
void pty_open(file_node_t *node);
void pty_close(file_node_t *node);
int pty_ioctl(file_node_t *node, unsigned long request, void *args);

#endif // NIGHTOS_PTY_H