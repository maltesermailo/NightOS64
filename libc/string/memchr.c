//
// Created by Jannik on 20.04.2024.
//
#include <string.h>

void* memchr(const void* s, int c, size_t n) {
    unsigned char* p = s;
    unsigned char ch = (unsigned char)c;

    for(size_t i = 0; i < n; i++) {
        if(p[i] == ch) {
            return (void*)(p+i);
        }
    }

    return NULL;
}