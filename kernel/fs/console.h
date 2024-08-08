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

void console_init();

#endif //NIGHTOS_CONSOLE_H
