//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_TIMER_H
#define NIGHTOS_TIMER_H

void timer_init();
void ksleep(int milliseconds);
unsigned long get_counter();

#endif //NIGHTOS_TIMER_H
