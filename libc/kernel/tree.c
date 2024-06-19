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

        tree_destroy_node(other);
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

/***
 * Inserts a child into the tree.
 * A special case is if node == NULL, then the entire tree is replaced.
 * To ensure that the tree is properly cleaned up, the previous root node will be destroyed.
 * This means it's values and nodes will be freed.
 * If it is desired to not cleanup the tree, use tree_set_root_node and cleanup the nodes yourself with tree_free_node.
 * @param tree the tree
 * @param node the node or NULL
 * @param value the value to insert
 * @return the new tree node
 */
tree_node_t* tree_insert_child(tree_t* tree, tree_node_t* node, void* value) {
    tree_node_t* tree_node = calloc(1, sizeof(tree_node_t));

    tree_node->value = value;

    list_t* children = list_create();
    tree_node->children = children;

    //Completely cleanup tree if replacing root node
    if(node == NULL) {
        if(tree->head) {
            tree_destroy_node(tree->head);

            tree->head = NULL;
        }
    }

    //Set new root if head is not set or cleaned up
    if(!tree->head) {
        tree_node->value = value;
        tree_set_root_node(tree, tree_node);

        return tree_node;
    }

    tree_node->parent = node;

    list_insert(node->children, tree_node);

    int height = 0;

    //Now calculate height, just go up the parent pointers
    for(tree_node_t* other = tree_node->parent; other != null; other = other->parent) {
        if(other == tree->head) {
            //If it is root, abort
            if(tree->height < height) {
                tree->height = height;
            }

            break;
        }
        height++;
    }

    return tree_node;
}

void tree_remove(tree_t* tree, tree_node_t* node) {
    if(!node->parent) {
        if(tree->head != node) {
            //SOMETHINGS PRETTY WRONG HERE
            return;
        }

        //It's the root, lets set it to null
        tree->head = null;
        tree->height = 0;

        return;
    }

    tree_node_t * parent = node->parent;

    for(list_entry_t* listEntry = parent->children->head; listEntry != null; listEntry = listEntry->next) {
        if(listEntry) {
            if(listEntry->value == node) {
                //found

                list_delete(parent->children, listEntry);
            }
        }
    }

    node->parent = null;

    tree_calculate_height(tree, tree->head, 0);
}

tree_node_t* tree_find_child(tree_t* tree, tree_node_t* node, void* value) {
    if(node->value == value) {
        return node;
    }

    for(list_entry_t* listEntry = node->children->head; listEntry != null; listEntry = listEntry->next) {
        if(listEntry) {
            tree_node_t* other = (tree_node_t*)listEntry->value;

            if(other->value == value) {
                return other;
            }

            tree_node_t* result = tree_find_child(tree, other, value);

            if(result != 0) {
                return result;
            }
        }
    }

    return 0;
}

tree_node_t* tree_find_child_root(tree_t* tree, void* value) {
    return tree_find_child(tree, tree->head, value);
}

void tree_dump_preorder(tree_node_t* node) {
    printf("Node points to 0x%x\n", node->value);
    printf("Node has %d children\n", node->children->length);

    for(list_entry_t* listEntry = node->children->head; listEntry != null; listEntry = listEntry->next) {
        if(listEntry) {
            tree_node_t* other = (tree_node_t*)listEntry->value;

            tree_dump_preorder(other);
        }
    }
}

void tree_dump(tree_t* tree) {
    if(!tree->head) {
        printf("Tree has no head!\n");

        return;
    }

    tree_dump_preorder(tree->head);
}