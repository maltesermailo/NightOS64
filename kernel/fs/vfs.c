//
// Created by Jannik on 07.04.2024.
//
#include "vfs.h"
#include <string.h>
#include "../../libc/include/kernel/tree.h"

file_node_t* root_node;
tree_t* file_tree;

int vfs_read(file_node_t* node, int offset, int len) {

}

const struct file_operations fileOperations = {
    .read = vfs_read;
};

void mount_directly(char* name, file_node_t* node) {
    if(strlen(name) == 1 && memcmp(name, "/", strlen(name))) {
        //Root file system, just load directories
        root_node = node;

        tree_insert_child(tree, NULL, root_node);
    }

    //
}

void register_mount(char* name, mount_func func) {
    file_node_t* mount = func(name);

    mount_directly(name, mount);
}

void mount_empty(char* name) {

}

void vfs_install() {
    file_tree = tree_create();

    root_node = calloc(1, sizeof(file_node_t));

    root_node.type = FILE_TYPE_MOUNT_POINT; //Is set to mount point, because mount point is the most free to change type
    root_node.id = 1; //id 1 will always be the root
    root_node.size = 0;
    root_node.name = "[root]";
    root_node.refcount = 0;
    root_node.file_ops = fileOperations;

    tree_insert_child(tree, NULL, root_node);

    //At this point, drivers would read the root file system
}