//
// Created by Jannik on 21.06.2024.
//
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Hi, from initd\n");

    while(1) {
        char input[256];

        scanf("%s", input);
        printf("%s\n", input);
    }
}
