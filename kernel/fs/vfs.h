//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_VFS_H
#define NIGHTOS_VFS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "../../libc/include/kernel/tree.h"

#define FILE_TYPE_FILE 0x0
#define FILE_TYPE_BLOCK_DEVICE 0x3
#define FILE_TYPE_VIRTUAL_DEVICE 0x4 //Same as CHAR_DEVICE on Linux distros
#define FILE_TYPE_DIR 0x5
#define FILE_TYPE_NAMED_PIPE 0x6
#define FILE_TYPE_MOUNT_POINT 0x7
#define FILE_TYPE_DEVICES 0x8
#define FILE_TYPE_KERNEL 0x9
#define FILE_TYPE_SOCKET 0x10

#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

struct FILE;
struct list_dir;

struct file_operations {
    int (*read) (struct FILE*, char*, size_t, size_t);
    int (*write) (struct FILE*, char*, size_t, size_t);
    size_t (*seek) (struct FILE*, size_t);
    void (*open) (struct FILE*, int);
    //Also synonymous for flush
    void (*close) (struct FILE*);
    int (*read_dir) (struct FILE*, struct list_dir*, int);
    bool (*mkdir) (struct FILE*, char*);
    struct FILE* (*find_dir) (struct FILE*, char*);
    int (*get_size) (struct FILE*);
    int (*chmod) (struct FILE*, int);
    int (*chown) (struct FILE*, unsigned int, unsigned int);
    bool (*create)(struct FILE*, char*, int);
    int (*ioctl) (struct FILE*, unsigned long, void*);
    int (*poll) (struct FILE*, int);
    struct FILE* (*rename)(struct FILE*, char*);
    int (*delete)(struct FILE*); //UNLINK
    int (*link)(struct FILE*, char*);
};

typedef struct FSStruct {
    void* owner; //A pointer to an identifying object for each mount.
} fs_struct_t;

//TODO: Add support for symbolic links
typedef struct FILE {
    char name[256];
    char full_path[4096];

    uint64_t id;

    uint64_t owner; //The owner id
    uint64_t owner_group; //The owner group id

    uint64_t access_mask;

    uint64_t type;
    uint64_t size;
    bool cached;
    void* fs; //File system specific data

    struct file_operations file_ops;

    int64_t ref_count;
} file_node_t;

typedef struct file_handle {
    file_node_t* fileNode;

    int mode;
    uint64_t offset;

    int flags;
} file_handle_t;

typedef struct list_dir {
    char name[256];

    uint64_t type;
    uint64_t size;
} list_dir_t;

typedef file_node_t* (*mount_func)(char*);

file_node_t* OpenStdIn();
file_node_t* OpenStdOut();

//File functions
file_node_t* open(char* filename, int mode);
file_node_t* create(char* filename, int mode);
file_node_t* mkdir(char* filename);
file_node_t* mkdir_vfs(char* filename);
file_node_t* resolve_path(char* cwd, char* file, file_node_t** outParent, char** outFileName);
int delete(char* filename);
int move_file(file_node_t* node, char* new_path, int flags);

int vfs_read_dir(struct FILE* node, struct list_dir* buffer, int count);

file_handle_t* create_handle(file_node_t*);

//File descriptor functions
void close(file_handle_t* file);
int seek(file_handle_t* file, size_t len);
int write(file_handle_t* file, char* bytes, size_t len);
int read(file_handle_t* file, char* buffer, size_t len);
int getdents(file_node_t* file, list_dir_t** buffer, int count);
int get_size(file_node_t* file);
int chmod(file_node_t* file, int mode);
int chown(file_node_t* file, unsigned int user, unsigned int group);
int ioctl(file_node_t* file, unsigned long request, void* args);
int fcntl(file_handle_t* file, int operation, void* data);
list_dir_t* find(char* filename);
int fpoll(file_handle_t* file, int events);
int link(file_handle_t* file, char* path);

char* get_full_path(file_node_t* node);

//Mount functions
int register_mount(char* name, mount_func func);
int mount_directly(char* name, file_node_t* root);
int mount_empty(char* name, int fileType);

bool insert_file(file_node_t* parent, file_node_t* new);

//Sets up the virtual file system
void vfs_install();
void vfs_teardown();

//Utility
int get_next_file_id();
struct file_operations* get_vfs_ops();

tree_t* debug_get_file_tree();

#endif //NIGHTOS_VFS_H
