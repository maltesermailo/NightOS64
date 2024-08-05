// pty.c

#include "pty.h"
#include <string.h>
#include <errno.h>
#include "../../libc/include/kernel/ring_buffer.h"
#include "../proc/process.h"

static struct pty_pair pty_pairs[MAX_PTY_PAIRS];

static struct file_operations pty_master_ops = {
        .read = pty_read,
        .write = pty_write,
        .open = pty_open,
        .close = pty_close,
        .ioctl = pty_ioctl,
};

static struct file_operations pty_slave_ops = {
        .read = pty_read,
        .write = pty_write,
        .open = pty_open,
        .close = pty_close,
        .ioctl = pty_ioctl,
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

    pair->data->input_buffer = ring_buffer_create(PTY_BUFFER_SIZE);
    pair->data->output_buffer = ring_buffer_create(PTY_BUFFER_SIZE);
    pair->data->term_settings = malloc(sizeof(struct termios));
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

int pty_write_to_input(int pty_index, const char* buffer, size_t size) {
    if (pty_index >= MAX_PTY_PAIRS) return -EINVAL;
    struct pty_data *pty = pty_pairs[pty_index].data;
    return ring_buffer_write(pty->input_buffer, size, buffer);
}

int pty_read(file_node_t *node, char *buffer, size_t size, size_t offset) {
    struct pty_data *pty = (struct pty_data *)node->fs;
    return ring_buffer_read(pty->input_buffer, size, buffer);
}

int pty_write(file_node_t *node, char *buffer, size_t size, size_t offset) {
    struct pty_data *pty = (struct pty_data *)node->fs;
    return ring_buffer_write(pty->output_buffer, size, buffer);
}

int pty_write_char_to_input(int pty_index, char c) {
    if (pty_index >= MAX_PTY_PAIRS) return -EINVAL;
    struct pty_data *pty = pty_pairs[pty_index].data;

    // Write the single character to the input buffer
    return ring_buffer_write(pty->input_buffer, 1, &c);
}

void pty_open(file_node_t *node) {
    // Implement any necessary initialization
}

void pty_close(file_node_t *node) {
    // Implement any necessary cleanup
}

int pty_ioctl(file_node_t *node, unsigned long request, void *args) {
    struct pty_data *pty = (struct pty_data *)node->fs;

    switch (request) {
        /*case TCGETS:
            memcpy(args, pty->term_settings, sizeof(struct termios));
            return 0;
        case TCSETS:
            memcpy(pty->term_settings, args, sizeof(struct termios));
            return 0;
            // ... other IOCTL commands ...*/
        default:
            return -ENOTTY;
    }
}