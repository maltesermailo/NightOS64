//
// Created by Jannik on 28.05.2024.
//

#ifndef NIGHTOS_RAMFS_H
#define NIGHTOS_RAMFS_H

#include <stdint.h>
#include "vfs.h"

//* This defines an in-ram filesystem. Every file created on this is flushed after restart. */

typedef struct RamFile {
    uintptr_t begin; //Start of the file in memory
    uintptr_t size; //Size of the buffer
} ram_file_t;

void ramfs_init(char* path);
file_node_t* ramfs_find_dir(file_node_t* node, char* name);

#endif //NIGHTOS_RAMFS_H
