//
// Created by Jannik on 07.04.2024.
//
#include "vfs.h"
#include <string.h>

file_node_t root_node;

int vfs_read(file_node_t* node, int offset, int len) {

}

const struct file_operations fileOperations = {
    .read = vfs_read;
};

void mount_directly(const char* name, mount_func func) {
    if(strlen(name) == 1 && memcmp(name, "/", strlen(name))) {
        //Root file system, just load directories
        file_node_t* fileNode = func(name);


    }
}

void vfs_install() {
    root_node.type = FILE_TYPE_MOUNT_POINT; //Is set to mount point, because mount point is the most free to change type
    root_node.id = 1; //id 1 will always be the root
    root_node.size = 0;
    root_node.name = "[root]";
    root_node.refcount = 0;
    root_node.file_ops = fileOperations;

    //At this point, drivers would read the root file system
}