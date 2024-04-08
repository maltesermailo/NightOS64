//
// Created by Jannik on 08.04.2024.
//
#include "../include/kernel/list.h"
#include "../../kernel/alloc.h"

list_t * list_create() {
    list_t* list = malloc(sizeof(list_t));

    list->length = 0;
    list->head = NULL;
    list->tail = NULL;

    return list;
}