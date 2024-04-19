//
// Created by Jannik on 13.04.2024.
//

#ifndef NIGHTOS_TREE_H
#define NIGHTOS_TREE_H

#include "list.h"
#include "../stdlib.h"

typedef struct TreeNode {
    void* value;

    list_t* children;
    tree_node_t* parent;
} tree_node_t;

typedef struct Tree {
    tree_node_t* head;
    size_t height;
} tree_t;

typedef struct AVLTreeNode {
    void* value;

    avl_tree_node_t* left;
    avl_tree_node_t* right;

    int height;
    int balanceFactor;
} avl_tree_node_t;

typedef struct AVLTree {
    avl_tree_node_t* root;
    size_t height;
} avl_tree_t;

tree_t* tree_create();
void tree_destroy(tree_t* tree);
void tree_destroy_node(tree_node_t* tree_node);
void tree_free(tree_t* tree);
void tree_free_node(tree_node_t* treeNode);
void tree_set_root_node(tree_t* tree, tree_node_t* root);
tree_node_t* tree_insert_child(tree_t* tree, tree_node_t* node, void* value);
void tree_remove(tree_t* tree, tree_node_t* node);
tree_node_t* tree_find_child(tree_t* tree, void* value);
tree_node_t* tree_find_child(tree_t* tree, tree_node_t* node, void* value);
void tree_dump(tree_t* tree);

avl_tree_t* avl_tree_create();
void avl_tree_destroy(avl_tree_t* tree);
void avl_tree_destroy_node(avl_tree_node_t * tree_node);
void avl_tree_set_root_node(avl_tree_t* tree, avl_tree_node_t * root);
avl_tree_node_t* avl_tree_insert_child(avl_tree_t * tree, avl_tree_node_t * node, void* value);
void avl_tree_remove(avl_tree_t* tree, avl_tree_node_t* node);
avl_tree_node_t* avl_tree_find_child(avl_tree_t* tree, void* value);


#endif //NIGHTOS_TREE_H
