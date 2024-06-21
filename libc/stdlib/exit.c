//
// Created by Jannik on 21.06.2024.
//
#include <stdlib.h>
#include <stdio.h>

void exit(int exitCode) {
    while(1);

    __builtin_unreachable();
}