// pty.c

#include "pty.h"
#include <string.h>
#include <errno.h>
#include "../../libc/include/kernel/ring_buffer.h"
#include "../proc/process.h"
#include "../terminal.h"
#include <stdint.h>
#include "../../mlibc/abis/linux/poll.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static struct pty_pair pty_pairs[MAX_PTY_PAIRS];

static struct file_operations pty_master_ops = {
        .read = pty_master_read,
        .write = pty_master_write,
        .open = pty_open,
        .close = pty_close,
        .ioctl = pty_ioctl,
        .poll = pty_poll
};

static struct file_operations pty_slave_ops = {
        .read = pty_slave_read,
        .write = pty_write,
        .open = pty_open,
        .close = pty_close,
        .ioctl = pty_ioctl,
        .poll = pty_poll
};

int pty_init(void) {
    memset(pty_pairs, 0, sizeof(pty_pairs));
    return 0;
}

int pty_create_pair(int pty_index) {
    if (pty_index >= MAX_PTY_PAIRS) return -EINVAL;

    struct pty_pair *pair = &pty_pairs[pty_index];
    pair->data = malloc(sizeof(struct pty_data));
    if (!pair->data) return -ENOMEM;

    pair->data->input_buffer = ring_buffer_create(PTY_BUFFER_SIZE, false);
    pair->data->output_buffer = ring_buffer_create(PTY_BUFFER_SIZE, false);
    pair->data->term_settings = calloc(1, sizeof(struct termios));
    pair->data->raw = false;
    pair->data->index = pty_index;

    mutex_init(&pair->wait_queue_read);
    mutex_init(&pair->wait_queue_write);

    if(pty_index == 0) {
        pair->data->console = true;
        pair->data->term_settings->c_lflag = ECHO | ICANON;
    }

    if (!pair->data->term_settings) {
        free(pair->data);
        return -ENOMEM;
    }
    // Initialize termios with default settings
    // init_default_termios(pair->data->term_settings);

    pair->data->session_leader = -1;

    // Initialize master node
    strcpy(pair->master.name, "pty_master");
    pair->master.type = FILE_TYPE_VIRTUAL_DEVICE;
    pair->master.fs = pair->data;
    pair->master.file_ops = pty_master_ops;

    // Initialize slave node
    strcpy(pair->slave.name, "pty_slave");
    pair->slave.type = FILE_TYPE_VIRTUAL_DEVICE;
    pair->slave.fs = pair->data;
    pair->slave.file_ops = pty_slave_ops;

    return 0;
}

file_node_t* pty_get_master(int pty_index) {
    if (pty_index >= MAX_PTY_PAIRS) return NULL;
    return &pty_pairs[pty_index].master;
}

file_node_t* pty_get_slave(int pty_index) {
    if (pty_index >= MAX_PTY_PAIRS) return NULL;
    struct pty_data *pty = pty_pairs[pty_index].data;

    if (pty->session_leader == -1) {
        pty->session_leader = get_current_process()->id;
    }

    return &pty_pairs[pty_index].slave;
}

static int find_newline(circular_buffer_t* buffer) {
    int available = ring_buffer_available(buffer);
    for (int i = 0; i < available; i++) {
        uint8_t c;
        if (ring_buffer_peek(buffer, i, &c) && c == '\n') {
            return i + 1;  // Return the position after the newline
        }
    }
    return -1;  // No newline found
}

int pty_slave_read(file_node_t *node, char *buffer, size_t size, size_t offset) {
    struct pty_data *pty = (struct pty_data *)node->fs;

    if(pty->term_settings->c_lflag & ICANON) {
        int newline_pos;

        while((newline_pos = find_newline(pty->input_buffer)) == -1) {
            mutex_acquire_if_free(&pty_pairs[pty->index].wait_queue_read);
            mutex_wait(&pty_pairs[pty->index].wait_queue_read);
        }

        return ring_buffer_read(pty->input_buffer, MIN(newline_pos, size), buffer);
    } else {
        return ring_buffer_read(pty->input_buffer, size, buffer);
    }
}

int pty_write(file_node_t *node, char *buffer, size_t size, size_t offset) {
    struct pty_data *pty = (struct pty_data *)node->fs;
    return ring_buffer_write(pty->output_buffer, size, buffer);
}

int pty_master_read(file_node_t *node, char *buffer, size_t size, size_t offset) {
    return 0;
}

int pty_master_write(file_node_t *node, char *buffer, size_t size, size_t offset) {
    struct pty_data *pty = (struct pty_data *)node->fs;
    return pty_write_to_input(pty->index, buffer, size);
}

static void echo_char(struct pty_data *pty, int pty_index, char c) {
    if (pty->console) {
        terminal_putchar(c);
        terminal_swap();
        __asm__ volatile ("mfence");
    } else {
        pty_write(pty_get_slave(pty_index), &c, 1, 0);
    }
}

static void signal_input_ready(struct pty_data *pty) {
    if(!mutex_acquire_if_free(&pty_pairs[pty->index].wait_queue_read)) {
        mutex_release(&pty_pairs[pty->index].wait_queue_read);
    }
}

int pty_write_to_input(int pty_index, const char* buffer, size_t size) {
    if (pty_index >= MAX_PTY_PAIRS) return -EINVAL;
    struct pty_data *pty = pty_pairs[pty_index].data;

    size_t written = 0;
    bool line_complete = false;

    for (size_t i = 0; i < size && !line_complete; i++) {
        char c = buffer[i];

        if (!(pty->term_settings->c_lflag & ICANON) || pty->raw) {
            // In raw mode or non-canonical mode, just write the character directly
            if (ring_buffer_write(pty->input_buffer, 1, &c) > 0) {
                if (pty->term_settings->c_lflag & ECHO) {
                    echo_char(pty, pty_index, c);
                }
                written++;
            } else {
                break;  // Buffer is full
            }
        } else {
            // In canonical mode, handle special characters
            switch (c) {
                case '\b':  // Backspace
                    if (ring_buffer_available(pty->input_buffer) > 0) {
                        ring_buffer_pop(pty->input_buffer);
                        if (pty->term_settings->c_lflag & ECHO) {
                            echo_char(pty, pty_index, '\b');
                            echo_char(pty, pty_index, ' ');
                            echo_char(pty, pty_index, '\b');
                        }
                    }
                    written++;
                    break;
                case '\n':  // Newline
                case '\r':  // Carriage return
                    if (ring_buffer_write(pty->input_buffer, 1, &c) > 0) {
                        if (pty->term_settings->c_lflag & ECHO) {
                            echo_char(pty, pty_index, c);
                        }
                        written++;
                        line_complete = true;
                    } else {
                        line_complete = true;  // Buffer is full
                    }
                    break;
                default:
                    if (ring_buffer_write(pty->input_buffer, 1, &c) > 0) {
                        if (pty->term_settings->c_lflag & ECHO) {
                            echo_char(pty, pty_index, c);
                        }
                        written++;
                    } else {
                        line_complete = true;  // Buffer is full
                    }
                    break;
            }
        }
    }

    // Signal that input is ready if we're not in canonical mode, or if we've completed a line
    if (!(pty->term_settings->c_lflag & ICANON) || pty->raw || line_complete) {
        signal_input_ready(pty);
    }

    return written;
}

int pty_write_char_to_input(int pty_index, char c) {
    return pty_write_to_input(pty_index, &c, 1);
}

void pty_open(file_node_t *node, int i) {
    // Implement any necessary initialization
}

void pty_close(file_node_t *node) {
    // Implement any necessary cleanup
}

int pty_ioctl(file_node_t *node, unsigned long request, void *args) {
    struct pty_data *pty = (struct pty_data *)node->fs;

    switch (request) {
        case TCGETS:
            memcpy(args, pty->term_settings, sizeof(struct termios));
            return 0;
        case TCSETS:
            memcpy(pty->term_settings, args, sizeof(struct termios));
            return 0;
        case TIOCGPGRP:
            *((uint32_t*)args) = pty->session_leader;
            return 0;
        case TIOCSPGRP:
            pty->session_leader = *(uint32_t*)args;
            return 0;
        default:
            return -ENOSYS;
    }
}

int pty_poll(file_node_t* node, int requested) {
    int revents = 0;

    struct pty_data *pty = (struct pty_data *)node->fs;

    if(requested & POLLIN) {
        if(find_newline(pty->input_buffer) != -1) {
            revents |= POLLIN;
        }
    }

    if(requested & POLLOUT) {
        //Not yet implemented
    }

    return revents;
}