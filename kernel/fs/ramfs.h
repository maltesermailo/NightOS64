//
// Created by Jannik on 28.05.2024.
//

#ifndef NIGHTOS_RAMFS_H
#define NIGHTOS_RAMFS_H

#include <stdint.h>
#include "vfs.h"

//* This defines an in-ram filesystem. Every file created on this is flushed after restart. */

typedef struct RamFile {
    void* owner;

    uintptr_t begin; //Start of the file in memory
    uintptr_t size; //Size of the buffer
} ram_file_t;

void ramfs_init(char* path);
file_node_t* ramfs_find_dir(file_node_t* node, char* name);
bool ramfs_mkdir(file_node_t* node, char* name);
int ramfs_read_dir(file_node_t* node, list_dir_t* entries, int max);
int ramfs_read(file_node_t* node, char* buffer, size_t offset, size_t length);
int ramfs_write(file_node_t* node, char* buffer, size_t offset, size_t length);
bool ramfs_create(file_node_t* node, char* name, int mode);
int ramfs_delete(file_node_t* node);
void ramfs_open(file_node_t* node, int mode);
void ramfs_close(file_node_t* node);
file_node_t* ramfs_rename(file_node_t* node, char* newpath);


#endif //NIGHTOS_RAMFS_H
