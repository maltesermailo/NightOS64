//
// Created by Jannik on 19.04.2024.
//

#include <stdbool.h>
#include <stddef.h>

#ifndef NIGHTOS_SERIAL_H
#define NIGHTOS_SERIAL_H

int serial_init();

int
serial_printf (const char *format, ...);
bool serial_print(const char* data, size_t length);

#endif //NIGHTOS_SERIAL_H
