//
// Created by Jannik on 20.06.2024.
//

#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include <stdio.h>
#include <string.h>

const char CONSOLE_NAME[] = "console0\0";

int console_input_read(struct FILE* node, char* buffer, size_t offset, size_t length) {
    return 0; // NOT YET IMPLEMENTED
}

int console_output_write(struct FILE* node, char* buffer, size_t offset, size_t length) {
    if(length > 2048) {
        return -1;
    }

    char printBuffer[length];
    memcpy(printBuffer, buffer, length);

    printf("%s", printBuffer);

    return length;
}

int console_output_seek(struct FILE* node, size_t offset) {
    return 0;
}

int console_ioctl(struct FILE* node, unsigned long operation, void* data) {
    return 0;
}

void console_init() {
    file_node_t* node = calloc(1, sizeof(file_node_t));
    node->id = get_next_file_id();
    node->refcount = 0;
    node->size = 0;
    node->type = FILE_TYPE_VIRTUAL_DEVICE;

    strncpy(node->name, CONSOLE_NAME, strlen(CONSOLE_NAME));

    node->file_ops.write = console_output_write;
    node->file_ops.ioctl = console_ioctl;

    mount_directly("/dev/console0", node);
}