//
// Created by Jannik on 20.06.2024.
//

#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include <stdio.h>
#include <string.h>
#include "../terminal.h"
#include "pty.h"
#include "../keyboard.h"

const char CONSOLE_NAME[] = "console0\0";

int console_input_read(struct FILE* node, char* buffer, size_t offset, size_t length) {
    return pty_slave_read(pty_get_slave(0), buffer, length, offset); // NOT YET IMPLEMENTED
}

int console_output_write(struct FILE* node, char* buffer, size_t offset, size_t length) {
    if(length > 2048) {
        return -1;
    }

    char printBuffer[length];
    memcpy(printBuffer, buffer, length);

    terminal_write(printBuffer, length);

    return length;
}

int console_output_seek(struct FILE* node, size_t offset) {
    return 0;
}

int console_ioctl(struct FILE* node, unsigned long operation, void* data) {
    if(operation == TISTTY) {
        return 1;
    }

    if(operation == 0x030) {
        //OWO operation
        uint16_t* terminalBuffer = (uint16_t*)0xB8000;

        for(int y = 0; y < 25; y++) {
            for(int x = 0; x < 80; x++) {
                const size_t index = y * 80 + x;
                terminalBuffer[index] = ~terminalBuffer[index];
            }
        }
    }

    return 0;
}

void key_event(key_event_t* event) {
    if(event->isDown) {
        pty_write_char_to_input(0, (char) event->keyCode);
    }
}

void console_init() {
    file_node_t* node = calloc(1, sizeof(file_node_t));
    node->id = get_next_file_id();
    node->ref_count = 0;
    node->size = 0;
    node->type = FILE_TYPE_VIRTUAL_DEVICE;

    strncpy(node->name, CONSOLE_NAME, strlen(CONSOLE_NAME));

    node->file_ops.write = console_output_write;
    node->file_ops.read = console_input_read;
    node->file_ops.ioctl = console_ioctl;

    pty_init();
    pty_create_pair(0);

    registerKeyEventHandler(key_event);

    mount_directly("/dev/console0", node);
}