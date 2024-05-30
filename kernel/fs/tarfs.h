//
// Created by Jannik on 16.05.2024.
//
#include "vfs.h"

#ifndef NIGHTOS_TARFS_H
#define NIGHTOS_TARFS_H

//Tar structure by Jason Lee
/*
tar.h
tar data structure and accompanying functions

Copyright (c) 2015 Jason Lee

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE
*/
typedef struct tar {
    uintptr_t begin;                     // location of data in file (including metadata)
    union {
        // UStar format (POSIX IEEE P1003.1)
        struct {
            char name[100];             // file name
            char mode[8];               // permissions
            char uid[8];                // user id (octal)
            char gid[8];                // group id (octal)
            char size[12];              // size (octal)
            char mtime[12];             // modification time (octal)
            char check[8];              // sum of unsigned characters in block, with spaces in the check field while calculation is done (octal)
            char type;                  // file type
            char link_name[100];   // name of linked file
            char ustar[8];              // ustar\000
            char owner[32];             // user name (string)
            char group[32];             // group name (string)
            char major[8];              // device major number
            char minor[8];              // device minor number
            char prefix[155];
        };

        char block[512];                    // raw memory (500 octets of actual data, padded to 1 block)
    };

    struct tar * next;
} tar_t;

typedef struct tar_filesystem {
    tar_t* root;
    file_node_t* root_node;
    int len;
} tar_filesystem_t;

typedef struct tar_context {
    tar_filesystem_t* fs;
    tar_t* entry;
} tar_context_t;

/**
 * Loads the specified tar buffer as a tar filesystem. The buffer has to be ready at all times, don't free it or the tar filesystem will crash.
 * @param path the path to mount to
 * @param tarptr the pointer to the memory buffer with the metadata and the files
 * @param size the size of the tar buffer
 */
void tarfs_init(char* path, void* tarptr, uintptr_t size);

/**
 * Mounts the tar file system at the specified path. This function is called by the VFS after tarfs_init is invoked.
 * @param name the path
 * @return the root node
 */
file_node_t* tarfs_mount(char* name);

file_node_t* tarfs_find_dir(file_node_t* node, char* name);
int tarfs_read(file_node_t* node, char* buf, size_t offset, size_t length);

#endif //NIGHTOS_TARFS_H
