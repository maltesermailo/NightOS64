//
// Created by Jannik on 20.06.2024.
//

#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include <stdio.h>
#include <string.h>
#include "../../mlibc/abis/linux/errno.h"
#include "../terminal.h"
#include "pty.h"
#include "../keyboard.h"
#include "../../mlibc/abis/linux/poll.h"

const char CONSOLE_NAME[] = "tty\0";
int terminalWidth = 0;
int terminalHeight = 0;

enum console_state {
    STATE_NORMAL,
    STATE_ESCAPE,
    STATE_BRACKET,
    STATE_PARSE_PARAMS
};

struct console_data {
    enum console_state state;
    int params[2];
    int param_idx;
};

static struct console_data console;

int console_input_read(struct FILE* node, char* buffer, size_t offset, size_t length) {
    return pty_slave_read(pty_get_slave(0), buffer, length, offset); // NOT YET IMPLEMENTED
}

void handle_escape_sequence(char c) {
    switch (c) {
        case 'H':
            if (console.param_idx == 0) {
                terminal_putchar('\r');
            } else {
                terminal_setcursor(console.params[0] - 1, console.params[1] - 1);
            }
            break;
        case 'J':
            if (console.params[0] == 2) {
                terminal_clear();
            }
            // Implement other clear screen options if needed
            break;
        case 'K':
            terminal_resetline();
            break;
            // Add more escape sequence handlers as needed
    }
}

void handle_console_char(char c) {
    switch (console.state) {
        case STATE_NORMAL:
            //ESCAPE
            if (c == 27) {
                console.state = STATE_ESCAPE;
            } else {
                terminal_putchar(c);
            }
            break;
        case STATE_ESCAPE:
            if (c == '[') {
                console.state = STATE_BRACKET;
                console.param_idx = 0;
                console.params[0] = console.params[1] = 0;
            } else {
                console.state = STATE_NORMAL;
            }
            break;
        case STATE_BRACKET:
            if (c >= '0' && c <= '9') {
                console.state = STATE_PARSE_PARAMS;
                console.params[console.param_idx] = c - '0';
            } else {
                handle_escape_sequence(c);
                console.state = STATE_NORMAL;
            }
            break;
        case STATE_PARSE_PARAMS:
            if (c >= '0' && c <= '9') {
                console.params[console.param_idx] = console.params[console.param_idx] * 10 + (c - '0');
            } else if (c == ';') {
                console.param_idx++;
            } else {
                handle_escape_sequence(c);
                console.state = STATE_NORMAL;
            }
            break;
    }
}

int console_output_write(struct FILE* node, char* buffer, size_t offset, size_t length) {
    if(length > 2048) {
        return -1;
    }

    for(int i = 0; i < length; i++) {
        handle_console_char(buffer[i]);
    }

    terminal_swap();

    return length;
}

int console_output_seek(struct FILE* node, size_t offset) {
    return 0;
}

int console_ioctl(struct FILE* node, unsigned long operation, void* data) {
    switch (operation) {
        case TISTTY:
            return 1;
        case TIOCGWINSZ:
            struct winsize* winsz = (struct winsize*)data;
            winsz->ws_col = terminalWidth;
            winsz->ws_row = terminalHeight;
            winsz->ws_xpixel = 0;
            winsz->ws_ypixel = 0;
            return 0;
        case TIOCSWINSZ:
            return -ENOSYS;
        default:
            return pty_ioctl(pty_get_slave(0), operation, data);
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

    return -EINVAL;
}

int console_poll(file_node_t* node, int requested) {
    int revents = 0;

    if(requested & POLLIN) {
        revents |= pty_get_slave(0)->file_ops.poll(pty_get_slave(0), requested);
    }

    if(requested & POLLOUT) {
        revents |= POLLOUT;
    }

    return revents;
}

void key_event(key_event_t* event) {
    if(event->isDown) {
        pty_write_char_to_input(0, (char) event->keyCode);
    }
}

void console_init(int terminalWidthIn, int terminalHeightIn) {
    console.state = STATE_NORMAL;

    file_node_t* node = calloc(1, sizeof(file_node_t));
    node->id = get_next_file_id();
    node->ref_count = 0;
    node->size = 0;
    node->type = FILE_TYPE_VIRTUAL_DEVICE;

    strncpy(node->name, CONSOLE_NAME, strlen(CONSOLE_NAME));

    node->file_ops.write = console_output_write;
    node->file_ops.read = console_input_read;
    node->file_ops.ioctl = console_ioctl;
    node->file_ops.poll = console_poll;

    pty_init();
    pty_create_pair(0);

    terminalWidth = terminalWidthIn;
    terminalHeight = terminalHeightIn;

    registerKeyEventHandler(key_event);

    mount_directly("/dev/tty", node);
}