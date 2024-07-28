//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_TIMER_H
#define NIGHTOS_TIMER_H

#include <stdint.h>
#include <bits/ansi/time_t.h>


struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

void timer_init();
void ksleep(long milliseconds);
unsigned long get_counter();

int wait(volatile uint32_t* mem, uint32_t bit, uint64_t timeout);

void panic();

#endif //NIGHTOS_TIMER_H
