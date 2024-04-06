//
// Created by Jannik on 06.04.2024.
//
#pragma once
#ifndef NIGHTOS_LOCK_H
#define NIGHTOS_LOCK_H

#include <stdatomic.h>

typedef atomic_flag spin_t;

static inline void spin_lock(spin_t * lock) {
    while(atomic_flag_test_and_set(lock)) {
        //Do nothing
        ;;
    }
}

static inline void spin_unlock(spin_t * lock) {
    atomic_flag_clear(lock);
}

#endif //NIGHTOS_LOCK_H
