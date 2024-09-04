//
// Created by Jannik on 08.07.2024.
//
#include "../mutex.h"
#include "../lock.h"
#include "../proc/process.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "../timer.h"
#include "../../mlibc/abis/linux/errno.h"

mutex_t* create_mutex() {
    mutex_t* mutex = calloc(1, sizeof(mutex_t));
    if (!mutex) return NULL;
    spin_unlock(&mutex->lock);

    mutex->owner = NULL;
    mutex->waiting = list_create();
}

void mutex_init(mutex_t* mutex) {
    spin_unlock(&mutex->lock);

    mutex->owner = NULL;
    mutex->waiting = list_create();
}

void mutex_acquire(mutex_t* mutex) {
    spin_lock(&mutex->lock);

    while(mutex->owner) {
        spin_unlock(&mutex->lock);
        wait_for_object(mutex);
        spin_lock(&mutex->lock);
    }
    mutex->owner = get_current_process();

    spin_unlock(&mutex->lock);
}

int mutex_acquire_timeout(mutex_t* mutex, unsigned long timeout_ms) {
    unsigned long start_time = get_counter();

    spin_lock(&mutex->lock);

    while(mutex->owner) {
        spin_unlock(&mutex->lock);
        if (wait_for_object_timeout(mutex, timeout_ms - (get_counter() - start_time)) != 0) {
            return -ETIMEDOUT;
        }
        spin_lock(&mutex->lock);

        if (get_counter() - start_time >= timeout_ms) {
            spin_unlock(&mutex->lock);
            return -ETIMEDOUT;
        }
    }

    mutex->owner = get_current_process();

    spin_unlock(&mutex->lock);
    return 0;
}

void mutex_wait(mutex_t* mutex) {
    spin_lock(&mutex->lock);

    while(mutex->owner) {
        spin_unlock(&mutex->lock);
        wait_for_object(mutex);
        spin_lock(&mutex->lock);
    }

    spin_unlock(&mutex->lock);
}

bool mutex_acquire_if_free(mutex_t* mutex) {
    spin_lock(&mutex->lock);

    if(mutex->owner) {
        spin_unlock(&mutex->lock);

        return false;
    }

    mutex->owner = get_current_process();
    spin_unlock(&mutex->lock);

    return true;
}

void mutex_release(mutex_t* mutex) {
    spin_lock(&mutex->lock);

    //Mutexes in kernel context will never clear if not owner
    //Mutexes in user space will use the futex api
    //if(mutex->owner != get_current_process()) {
    //    printf("Non-owner tried to clear mutex");
    //    panic();
    //    return;
    //}

    mutex->owner = NULL;
    wakeup_waiting(mutex->waiting);
    spin_unlock(&mutex->lock);
}