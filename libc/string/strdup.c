//
// Created by Jannik on 20.04.2024.
//
#include <string.h>
#include <stdlib.h>

char* strdup(const char* str) {
    size_t len = strlen(str);

    void* new = malloc(len+1);
    memcpy(new, str, len+1);

    return (char*)new;
}
