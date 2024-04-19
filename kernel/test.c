//
// Created by Jannik on 19.04.2024.
//

#include "test.h"
#include "../libc/include/kernel/list.h"
#include "../libc/include/kernel/tree.h"

void list_test() {
    list_t* list = list_create();
    printf("List size is %d\n", list->length);
    printf("List pointer is 0x%x\n", list);

    int* test1 = malloc(sizeof(int));
    (*test1) = 42;

    int* test2 = malloc(sizeof(int));
    (*test2) = 45;

    int* test3 = malloc(sizeof(int));
    (*test3) = 48;

    list_insert(list, test1);
    list_insert(list, test2);
    list_entry_t* head = list->head;

    list_insert_before(list, head, test3);

    list_entry_t* entry = list_find(list, test2);
    if(entry->value == test2) {
        printf("Test succeeded\n");
    }

    printf("List pointer is 0x%x\n", list);

    list_dump(list);
}

void tree_test() {
    tree_t* tree = tree_create();

    int* test1 = malloc(sizeof(int));
    (*test1) = 42;

    int* test2 = malloc(sizeof(int));
    (*test2) = 45;

    int* test3 = malloc(sizeof(int));
    (*test3) = 48;

    int* test4 = malloc(sizeof(int));
    (*test4) = 51;

    tree_node_t* root = tree_insert_child(tree, null, test1);
    tree_node_t* find = tree_insert_child(tree, root, test2);

    tree_node_t* subnode = tree_insert_child(tree, root, test3);
    tree_insert_child(tree, subnode, test4);

    tree_node_t* result = tree_find_child(tree, test2);

    if(result == find) {
        printf("Test successful");
    }

    tree_dump(tree);
}