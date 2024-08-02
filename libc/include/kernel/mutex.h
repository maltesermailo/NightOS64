//
// Created by Jannik on 08.07.2024.
//

#pragma once
#ifndef NIGHTOS_MUTEX_H
#define NIGHTOS_MUTEX_H

#include "../../../kernel/lock.h"
#include <stdbool.h>
#include "list.h"

typedef struct mutex {
    spin_t lock;
    void* owner;
    list_t* waiting;
} mutex_t;

/**
 * Provided by kernel
 */
extern mutex_t* create_mutex();
extern void mutex_acquire(mutex_t* mutex);
extern void mutex_wait(mutex_t* mutex);
extern bool mutex_acquire_if_free(mutex_t* mutex);
extern void mutex_release(mutex_t* mutex);

#endif //NIGHTOS_MUTEX_H
