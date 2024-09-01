//
// Created by Jannik on 19.06.2024.
//

#ifndef NIGHTOS_CONSOLE_H
#define NIGHTOS_CONSOLE_H

#include "vfs.h"

struct console_pos {
    uint8_t x;
    uint8_t y;
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;  /* unused */
    unsigned short ws_ypixel;  /* unused */
};

void console_init(int, int);

#endif //NIGHTOS_CONSOLE_H
