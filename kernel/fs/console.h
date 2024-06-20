//
// Created by Jannik on 19.06.2024.
//

#ifndef NIGHTOS_CONSOLE_H
#define NIGHTOS_CONSOLE_H

#include "vfs.h"

#define IOCTL_SET_CURSOR_POS 1
#define IOCTL_GET_CURSOR_POS 2
#define IOCTL_CLEAR_SCREEN   3

struct console_pos {
    uint8_t x;
    uint8_t y;
};

void console_init();

#endif //NIGHTOS_CONSOLE_H
