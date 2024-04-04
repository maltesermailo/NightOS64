//
// Created by Jannik on 02.04.2024.
//
#include "../../timer.h"
#include "io.h"
#include "../../idt.h"
#include "../../terminal.h"

#define PIT0 0x40
#define PIT1 0x41
#define PIT2 0x42
#define PIT_CMD 0x43

#define PIT_MASK 0xFF
#define PIT_SCALE 1193180

static uint64_t counter = 0;

void pit_interrupt(regs_t* regs) {
    counter++;

    if(counter % 100 == 0) {
        printf("1s passed\n");
    }

    pic_sendEOI(0);
}

void timer_init() {
    irq_install_handler(0, pit_interrupt);

    int counter = PIT_SCALE / 100;
    outb(PIT_CMD, 0x34);
    outb(PIT0, counter & PIT_MASK);
    outb(PIT0, counter >> 8);
}