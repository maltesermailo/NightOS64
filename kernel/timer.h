//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_TIMER_H
#define NIGHTOS_TIMER_H

#include <stdint.h>

void timer_init();
void ksleep(int milliseconds);
void sleep(int milliseconds);
unsigned long get_counter();

int wait(volatile uint32_t* mem, uint32_t bit, uint64_t timeout);

void panic();

#endif //NIGHTOS_TIMER_H
