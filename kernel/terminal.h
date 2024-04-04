//
// Created by Jannik on 16.03.2024.
//

#ifndef NIGHTOS_TERMINAL_H
#define NIGHTOS_TERMINAL_H

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_resetline();

char* raw_itoa(unsigned int i);

void
printf (const char *format, ...);

#endif //NIGHTOS_TERMINAL_H
