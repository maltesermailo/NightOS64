//
// Created by Jannik on 17.05.2024.
//
#include "tarfs.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

//Adapted tar from Jason Lee
/*
tar.c
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

tar_t* archive;

int oct2bin(char *str, int size) {
    int n = 0;
    char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

bool isBufferZero(char* buf, int size) {
    for(int i = 0; i < size; i++) {
        if(buf[i] != 0) {
            return false;
        }
    }

    return true;
}

int read_mem(void* ptr, char* buf, int size) {
    char* ptrBuf = (char*) ptr;

    memcpy(buf, ptrBuf, size);

    if(isBufferZero(buf, size)) {
        return 0;
    }

    return size;
}

int tarfs_read(file_node_t* node, char* buf, size_t offset, size_t length) {
    //Grab the context
    tar_context_t* context = (tar_context_t*)node->fs;

    //Check if no tar entry
    if(context->entry == NULL) {
        return 0;
    }

    tar_t* entry = context->entry;

    uintptr_t pointer = entry->begin;
    int size = oct2bin(entry->size, 11);
    uintptr_t end = pointer + size;

    //If offset is ahead of file
    if(offset > size) {
        return 0;
    }

    if(offset + length > size) {
        length = size - offset;
    }

    pointer += offset;

    char* ptr = (char*) pointer;

    memcpy(buf, ptr, length);

    return length;
}

file_node_t* tarfs_find_dir(file_node_t* node, char* name) {
    //Grab the context
    tar_context_t* context = (tar_context_t*)node->fs;

    int id = 0;

    if(node->type == FILE_TYPE_MOUNT_POINT) {
        //Root
        tar_t* current = context->fs->root;

        while(current) {
            char filename[256];
            memset(filename, 0, 256);
            strncat(filename, current->prefix, 155);
            strncat(filename, current->name, 100);

            int count = 0;
            for(int i = 0; filename[i]; i++) {
                if(filename[i] == '/') {
                    count++;
                }
            }

            if(count <= 1) {
                if(strcmp(filename, name) == 0) {
                    tar_context_t* node_context = calloc(1, sizeof(tar_context_t));
                    node_context->fs = context->fs;
                    node_context->entry = current;

                    file_node_t* node = calloc(1, sizeof(file_node_t));
                    strncpy(node->name, name, strlen(name));
                    strncpy(node->full_path, filename, strlen(filename));
                    node->id = id;
                    node->type = current->type == '0' ? FILE_TYPE_FILE : FILE_TYPE_DIR;
                    node->size = current->size;
                    node->refcount = 0;
                    node->fs = node_context;

                    return node;
                }
            }

            current = current->next;
            id++;
        }

        return null;
    } else {
        tar_t* current = context->fs->root;

        while(current) {
            char filename[256];
            strncat(filename, current->prefix, 155);
            strncat(filename, current->name, 100);

            //The sub path inside the tar file system, because I like mounting at subdirectories.
            char* path = strstr(name, context->fs->root_node->full_path);

            //Path is as long as root path, what happened?
            if(strlen(path) <= strlen(context->fs->root_node->full_path)) {
                return NULL;
            }

            path += strlen(context->fs->root_node->full_path);

            printf("Finding in %s", path);

            int count = 0;
            for(int i = 0; filename[i]; i++) {
                if(filename[i] == '/') {
                    count++;
                }
            }

            if(count <= 1) {
                if(strcmp(filename, path) == 0) {
                    tar_context_t* node_context = calloc(1, sizeof(tar_context_t));
                    node_context->fs = context->fs;
                    node_context->entry = current;

                    file_node_t* node = calloc(1, sizeof(file_node_t));
                    strncpy(node->name, name, strlen(name));
                    strncpy(node->full_path, filename, strlen(filename));
                    node->id = id;
                    node->type = current->type == '0' ? FILE_TYPE_FILE : FILE_TYPE_DIR;
                    node->size = current->size;
                    node->refcount = 0;
                    node->fs = node_context;

                    return node;
                }
            }

            current = current->next;
            id++;
        }

        return NULL;
    }
}

void tar_free(tar_t* ptr) {
    while(ptr) {
        tar_t* next = ptr->next;
        free(ptr);
        ptr = next;
    }
}

file_node_t* tarfs_mount(char* name) {
    char* file_name = strrchr(name, '/'); //Get file name

    file_node_t* root = calloc(1, sizeof(file_node_t));

    root->id = 0; // is the root
    root->size = 0; //Is a directory
    root->type = FILE_TYPE_MOUNT_POINT; //We set it to mount point so the driver knows its the root of the tar filesystem
    strncpy(root->name, file_name, strlen(file_name));
    strncpy(root->full_path, name, strlen(name));
    root->refcount = 0;
    root->file_ops->find_dir = tarfs_find_dir;
    root->file_ops->read = tarfs_read;

    //Create the tar filesystem structure
    tar_filesystem_t* fs = calloc(1, sizeof(tar_filesystem_t));
    fs->root = archive;
    fs->root_node = root;

    tar_t* current = fs->root;

    int len;
    while(current) {
        len++;

        current = current->next;
    }

    fs->len = len;

    //Create the tar context which holds a reference to the tar filesystem structure and the entry for this file, for root its null
    tar_context_t* context = calloc(1, sizeof(tar_context_t));
    context->fs = fs;
    context->entry = NULL;

    root->fs = context;

    return root;
}

void tarfs_init(char* path, void* ptr, uintptr_t bufsize) {
    tar_t* tar = NULL;
    tar_t* current = NULL;
    tar_t* previous = NULL;

    char* buf = (char*) ptr;

    bool read = true;

    while(read && buf < ((uintptr_t)ptr + bufsize)) {
        previous = current;
        current = calloc(1, sizeof(tar_t));

        if(read && read_mem(buf, current->block, 512) == 0) {
            buf += 512;

            if(read_mem(buf, current->block, 512) == 0) {
                tar_free(current);
                tar_free(tar);

                return;
            }

            read = false;
        }

        if(tar == NULL) {
            tar = current;
        }

        tar->begin = buf;

        int jump = oct2bin(current->size, 11);
        if(jump % 512) {
            jump += 512 - (jump % 512);
        }

        buf += 512 + jump;

        if(previous != NULL) {
            previous->next = current;
        }
    }

    //After this the tar should be completely loaded in, starting at the tar pointer. Now we load the file structure into memory by using the mount function.
    archive = tar;

    register_mount(path, tarfs_mount);
}