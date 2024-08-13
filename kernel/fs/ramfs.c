//
// Created by Jannik on 17.06.2024.
//

#include "vfs.h"
#include "ramfs.h"
#include "../terminal.h"
#include <string.h>
#include <abi-bits/fcntl.h>
#include "../proc/process.h"

void ramfs_init(char* path) {
    char* file_name = strrchr(path, '/'); //Get file name

    file_node_t* root = calloc(1, sizeof(file_node_t));

    root->id = get_next_file_id(); // is the root
    root->size = 0; //Is a directory
    root->type = FILE_TYPE_MOUNT_POINT; //We set it to mount point so the driver knows its the root of the tar filesystem
    strncpy(root->name, file_name, strlen(file_name));
    strncpy(root->full_path, path, strlen(path));
    root->ref_count = 0;
    root->file_ops.delete = ramfs_delete;
    root->file_ops.mkdir = ramfs_mkdir;
    root->file_ops.delete = ramfs_delete;
    root->file_ops.create = ramfs_create;
    root->file_ops.read = ramfs_read;
    root->file_ops.write = ramfs_write;

    mount_directly(path, root);
}

bool ramfs_mkdir(file_node_t* parent, char* dirname) {
    if(strlen(dirname) == 1 && memcmp(dirname, "/", strlen(dirname))) {
        return false;
    }

    tree_node_t* treeNode = tree_find_child_root(debug_get_file_tree(), parent);

    if(!treeNode) {
        printf("Warning: File %s is not present in file tree\n", parent->name);
        return false;
    }

    file_node_t* node = calloc(1, sizeof(file_node_t));

    node->type = FILE_TYPE_DIR; //Is set to mount point, because mount point is the most free to change type
    node->id = get_next_file_id(); //id 1 will always be the root
    node->size = 0;
    strncpy(node->name, dirname, strlen(dirname));
    node->ref_count = 0;
    node->file_ops.delete = ramfs_delete;
    node->file_ops.mkdir = ramfs_mkdir;
    node->file_ops.create = ramfs_create;
    node->file_ops.read = ramfs_read;
    node->file_ops.write = ramfs_write;
    node->file_ops.rename = ramfs_rename;
    parent->size++;

    tree_insert_child(debug_get_file_tree(), treeNode, node);

    return node;
}

int ramfs_read(file_node_t* node, char* buffer, size_t offset, size_t length) {
    ram_file_t* ramFile = (ram_file_t*)node->fs;

    char* file = (char*)ramFile->begin;

    if(offset > ramFile->size) {
        return 0;
    }

    file += offset;

    if((offset + length) > (ramFile->size)) {
        length = offset + length - ramFile->size;
    }

    memcpy(buffer, file, length);

    return length;
}

int ramfs_write(file_node_t* node, char* buffer, size_t offset, size_t length) {
    ram_file_t* ramFile = (ram_file_t*)node->fs;

    char* file = (char*)ramFile->begin;

    if(offset > ramFile->size) {
        char* newBuffer = malloc(offset+length);

        ramFile->begin = (uintptr_t)newBuffer;
        memcpy(newBuffer, file, ramFile->size);
        file = (char*)ramFile->begin;
    }

    if((offset + length) > ramFile->size) {
        char* newBuffer = malloc(offset+length);

        ramFile->begin = (uintptr_t)newBuffer;
        memcpy(newBuffer, file, ramFile->size);
        file = (char*)ramFile->begin;
    }

    file += offset;
    memcpy(file, buffer, length);

    if(node->size < offset + length) {
        node->size = offset + length;
    }

    return length;
}
bool ramfs_create(file_node_t* parent, char* name, int mode) {
    tree_node_t* treeNode = tree_find_child_root(debug_get_file_tree(), parent);

    if(!treeNode) {
        printf("Warning: File %s is not present in file tree\n", parent->name);
        return false;
    }

    file_node_t* node = calloc(1, sizeof(file_node_t));

    node->type = FILE_TYPE_FILE; //Is set to mount point, because mount point is the most free to change type
    node->id = get_next_file_id(); //id 1 will always be the root
    node->size = 0;
    strncpy(node->name, name, strlen(name));
    node->ref_count = 0;
    node->file_ops.delete = ramfs_delete;
    node->file_ops.create = ramfs_create;
    node->file_ops.read = ramfs_read;
    node->file_ops.write = ramfs_write;
    node->file_ops.rename = ramfs_rename;
    parent->size++;

    ram_file_t* ramFile = calloc(1, sizeof(ram_file_t));
    ramFile->begin = (uintptr_t) malloc(4096);
    ramFile->size = 4096;
    ramFile->owner = ramFile;

    node->fs = ramFile;

    tree_insert_child(debug_get_file_tree(), treeNode, node);

    return true;
}

int ramfs_delete(file_node_t* node) {
    ram_file_t* ramFile = (ram_file_t*) node->fs;

    free((void*)ramFile->begin);
    free(ramFile);

    return 0;
}

void ramfs_open(file_node_t* node, int mode) {
    if(mode & O_TRUNC) {
        ram_file_t* ramFile = (ram_file_t*) node->fs;
        memset((void*)ramFile->begin, 0, ramFile->size);
    }
}

file_node_t* ramfs_rename(file_node_t* node, char* path) {
    file_node_t* parent;
    resolve_path(get_cwd_name(), path, &parent, NULL);

    if(parent == NULL) {
        return NULL;
    }

    tree_node_t* treeNode = tree_find_child_root(debug_get_file_tree(), parent);

    if(!treeNode) {
        printf("Warning: File %s is not present in file tree\n", parent->name);
        return NULL;
    }

    tree_insert_child(debug_get_file_tree(), treeNode, node);

    return node;
}