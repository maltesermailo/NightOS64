//
// Created by Jannik on 14.04.2024.
//
#include "../include/kernel/tree.h"
#include "../include/stdlib.h"

tree_t* tree_create() {
    tree_t* tree = calloc(1, sizeof(tree_t));

    tree->head = NULL;
    tree->height = 0;

    return tree;
}

void tree_destroy(tree_t* tree) {
    tree_destroy_node(tree->head);

    free(tree);
}

void tree_destroy_node(tree_node_t* node) {
    if(node == NULL) {
        return;
    }

    for(list_entry_t* entry = node->children->head; entry != NULL; entry = entry->next) {
        tree_node_t* other = (tree_node_t*)entry->value;

        tree_free_node(other);
    }

    list_free(node->children);
    free(node->value);
    free(node);
}

void tree_free(tree_t* tree) {
    tree_free_node(tree->head);

    free(tree);
}

void tree_free_node(tree_node_t* node) {
    if(node == NULL) {
        return;
    }

    for(list_entry_t* entry = node->children->head; entry != NULL; entry = entry->next) {
        tree_node_t* other = (tree_node_t*)entry->value;

        tree_free_node(other);
    }

    list_free(node->children);
    free(node);
}

void tree_calculate_height(tree_t* tree, tree_node_t* node, int height) {
    for(list_entry_t* entry = node->children->head; entry != NULL; entry = entry->next) {
        tree_node_t* other = (tree_node_t*)entry->value;

        tree_calculate_height(tree, other, height+1);
    }

    if(height > tree->height) {
        tree->height = height;
    }
}

void tree_set_root_node(tree_t* tree, tree_node_t* node) {
    tree->head = node;
    tree_calculate_height(tree, node, 1);
}

tree_node_t* tree_insert_child(tree_t* tree, tree_node_t* node, void* value) {

}

void tree_remove(tree_t* tree, tree_node_t* node) {

}

tree_node_t* tree_find_child(tree_t* tree, void* value) {

}