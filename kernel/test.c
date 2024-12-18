//
// Created by Jannik on 19.04.2024.
//

#include <string.h>
#include "test.h"
#include "../libc/include/kernel/list.h"
#include "../libc/include/kernel/tree.h"
#include "fs/vfs.h"
#include "serial.h"
#include "terminal.h"
#include "memmgr.h"

void kmalloc_test() {
    void* ptr1 = kmalloc(32);
    void* ptr2 = kmalloc(45);

    printf("[KMALLOC_TEST] PTR1 is 0x%x", ptr1);
    printf("[KMALLOC_TEST] PTR2 is 0x%x", ptr2);

    kfree(ptr1);
    kfree(ptr2);
}

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

    printf("Inserting root\n");

    tree_node_t* root = tree_insert_child(tree, 0, test1);

    printf("Inserting child\n");

    tree_node_t* find = tree_insert_child(tree, root, test2);

    printf("Inserting last child\n");

    tree_node_t* subnode = tree_insert_child(tree, root, test3);
    tree_insert_child(tree, subnode, test4);

    tree_node_t* result = tree_find_child_root(tree, test2);

    if(result == find) {
        printf("Test successful\n");
    }

    tree_dump(tree);
}

void print_fs_tree(tree_node_t* treeNode, int depth) {
    //Gonna print this on serial.
    for(int i = 0; i < depth; i++) {
        serial_printf("-");
    }

    file_node_t* node = treeNode->value;

    serial_printf("%s, %d, %d\n", node->name, node->size, node->type);

    if(node->type == FILE_TYPE_DIR || node->type == FILE_TYPE_MOUNT_POINT) {
        if(node->size > 0) {
            for(list_entry_t* entry = treeNode->children->head; entry != NULL; entry = entry->next) {
                tree_node_t* subNode = entry->value;

                if(subNode) {
                    print_fs_tree(subNode, depth+1);
                }
            }
        }
    }
}

void vfs_test() {
    file_node_t* root = open("/", 0); //This should return the root
    tree_t* tree = debug_get_file_tree();

    tree_node_t* node = tree->head;

    print_fs_tree(node, 0); //Prints only in ram tree. Tar filesystem and others will not show up until read by read_dir

    //Now do the same with read_dir in the root directory
    int count = get_size(root);

    list_dir_t* ptr = NULL;
    int readCount = getdents(root, &ptr, count);

    printf("Read %d entries from root directory\n", readCount);

    for(int i = 0; i < readCount; i++) {
        printf("Entry: %s, %d, %d\n", ptr[i].name, ptr[i].type, ptr[i].size);
    }

    free(ptr);

    file_node_t* test = open("/test.txt", 0);
    file_handle_t* handle = create_handle(test);

    int testSize = get_size(test);
    printf("Test file size %d\n", testSize);

    char* bytes = malloc(testSize+1);
    memset(bytes, 0, testSize+1);
    read(handle, bytes, testSize);

    printf("Test: %d\n", strlen(bytes));
    printf("%s: %s\n", test->name, bytes);

    printf("Test successful if output is correct.\n");
}

void fat_test() {
    file_node_t* fatRoot = open("/mnt", 0);

    int count = get_size(fatRoot);
    printf("Fat Root Directory: %d entries\n", count);

    list_dir_t* ptr = NULL;
    int readCount = getdents(fatRoot, &ptr, count);

    printf("Read %d entries from root directory\n", readCount);

    for(int i = 0; i < readCount; i++) {
        printf("Entry: %s, %d, %d\n", ptr[i].name, ptr[i].type, ptr[i].size);
    }
}