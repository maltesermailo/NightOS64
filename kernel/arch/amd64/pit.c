//
// Created by Jannik on 02.04.2024.
//
#include "../../timer.h"
#include "io.h"
#include "../../idt.h"
#include "../../terminal.h"
#include "../../proc/process.h"

#define PIT0 0x40
#define PIT1 0x41
#define PIT2 0x42
#define PIT_CMD 0x43

#define PIT_MASK 0xFF
#define PIT_SCALE 1193180

static volatile uint64_t counter = 0;

/**
 * This function is called every 10 milliseconds and starts the context switch with schedule(false);
 * @param regs
 */
void pit_interrupt(regs_t* regs) {
    counter++;

    pic_sendEOI(0);
    if(regs->cs == 0x08) return;

    //We got pre-empted, so no sleep
    wakeup_sleeping();
    schedule(false);
}

/**
 * Sleeps for x milliseconds in busy-waiting, thereby blocking the cpu
 * @param milliseconds the milliseconds to wait
 */
void ksleep(int milliseconds) {
    int endCounter = (milliseconds / 10) + 1 + counter;

    while(1) {
        if(counter >= endCounter) {
            return;
        }

        __asm__ volatile("hlt");
    }
}

unsigned long get_counter() {
    return counter;
}

/**
 * This function initializes the PIT timer with a scale of approximately every 10 milliseconds
 */
void timer_init() {
    irq_install_handler(0, pit_interrupt);

    int counter = PIT_SCALE / 100;
    outb(PIT_CMD, 0x34);
    outb(PIT0, counter & PIT_MASK);
    outb(PIT0, counter >> 8);
}