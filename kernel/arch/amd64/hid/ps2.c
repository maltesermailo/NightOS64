//
// Created by Jannik on 02.04.2024.
//
#include "../../../keyboard.h"
#include "../../../idt.h"
#include "ps2.h"
#include "../io.h"
#include <stdbool.h>
#include "../../../terminal.h"
#include "../../../alloc.h"

static keyboard_event_handler_t keyboardEventHandler[64];
static keyboard_state_t state;

char kbd[59] = {
        0, 27,
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
        '-', '=', '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
        '[', ']', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
        0, '\\',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/',
        0, '*', 0, ' ', 0

};

void registerKeyEventHandler(keyboard_event_handler_t handler) {
    for(int i = 0; i < 64; i++) {
        if(keyboardEventHandler[i] == 0) {
            keyboardEventHandler[i] = handler;
            break;
        }
    }
}

void keyboard_handler(regs_t* regs) {
    unsigned char code = inb(0x60);
    pic_sendEOI(1);

    if(code == 0xE0) {
        state.kbd_extended_state = 1;
    }

    char keyCode = kbd[code];
    bool isup = code & KEY_UP_MASK;

    switch(code) {
        case KEY_CTRL:
            if(state.kbd_extended_state)
                state.rctrl = !isup;
            else
                state.lctrl = !isup;
            break;
        case KEY_LSHIFT:
            state.lshift = !isup;
            break;
    }

    if(state.kbd_extended_state == 0) {
        bool shift = state.lshift || state.rshift;

        if(!shift && keyCode >= 65)
            keyCode += 32;

        if(!isup) {
            key_event_t* keyEvent = malloc(sizeof(key_event_t));

            keyEvent->isDown = true;
            keyEvent->keyCode = keyCode;
            for(int i = 0; i < 64; i++) {
                if(keyboardEventHandler[i]) {
                    keyboardEventHandler[i](keyEvent);
                }
            }

            free(keyEvent);
        }
    } else {

    }
}

void ps2_init() {
    irq_install_handler(1, keyboard_handler);
}