//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_VFS_H
#define NIGHTOS_VFS_H
#include <stdbool.h>
#include <stddef.h>

typedef struct file_operations {
    int (*read) (file_node_t*, char *, size_t, size_t);
    int (*write) (file_node_t*, char**, size_t, size_t);
    size_t (*seek) (file_node_t*, size_t);
    void (*open) (file_node_t*);
    void (*close) (file_node_t*);
    void (*read_dir) (file_node_t*, file_node_t**);
    bool (*mkdir) (file_node_t*, char*);
    void (*find_dir) (file_node_t*, char*);
    int (*get_size) (file_node_t*);
    int (*chmod) (file_node_t*, int);
    int (*chown) (file_node_t*, unsigned int, unsigned int);
    bool (*create)(file_node_t*, char*, int);
    int (*ioctl) (file_node_t*, unsigned long, void*);
};

typedef struct FILE {
    char name[256];

    uint64_t id;
    uint64_t type;
    uint64_t size;

    const struct file_operations* file_ops;

    int64_t refcount;
} file_node_t;

typedef struct list_dir {
    char name[256];

    uint64_t type;
    uint64_t size;
} list_dir_t;

FILE* OpenFile(char* path);
FILE* OpenStdIn();
FILE* OpenStdOut();

unsigned long open(char* filename, int mode);
unsigned long create(char* filename, int mode);
unsigned long mkdir(char* filename);
void close(FILE* file);
int seek(FILE* file, size_t len);
int write(FILE* file, char* bytes, size_t len);
int read(FILE* file, char** buffer, size_t len);
int getdents(FILE* file, list_dir_t** buffer, int count);
int get_size(FILE* file);
int chmod(FILE* file, int mode);
int chown(FILE* file, unsigned int user, unsigned int group);
int ioctl(FILE* file, unsigned long request, void* args);
list_dir_t* find(char* filename);

#endif //NIGHTOS_VFS_H
