//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_VFS_H
#define NIGHTOS_VFS_H
#include <stdbool.h>
#include <stddef.h>

#define FILE_TYPE_FILE 0x1
#define FILE_TYPE_DIR 0x2
#define FILE_TYPE_BLOCK_DEVICE 0x3
#define FILE_TYPE_VIRTUAL_DEVICE 0x4
#define FILE_TYPE_MOUNT_POINT 0x5
#define FILE_TYPE_DEVICES 0x6
#define FILE_TYPE_KERNEL 0x7

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

typedef file_node_t* (*mount_func)(char*);

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

FILE* OpenStdIn();
FILE* OpenStdOut();

//File functions
unsigned long open(char* filename, int mode);
unsigned long create(char* filename, int mode);
unsigned long mkdir(char* filename);

//File descriptor functions
void close(FILE* file);
int seek(FILE* file, size_t len);
int write(FILE* file, char* bytes, size_t len);
int read(FILE* file, char** buffer, size_t len);
int getdents(FILE* file, list_dir_t** buffer, int count);
int get_size(FILE* file);
int chmod(FILE* file, int mode);
int chown(FILE* file, unsigned int user, unsigned int group);
int ioctl(FILE* file, unsigned long request, void* args);
int fcntl(FILE* file);
list_dir_t* find(char* filename);

//Mount functions
void register_mount(char* name, mount_func func);
void mount_directly(char* name, file_node_t* root);
void mount_empty(char* name, int fileType);

//Sets up the virtual file system
void vfs_install();

#endif //NIGHTOS_VFS_H
