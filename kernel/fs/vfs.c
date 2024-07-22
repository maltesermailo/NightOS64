//
// Created by Jannik on 07.04.2024.
//
#include "vfs.h"
#include "../proc/process.h"
#include "../../libc/include/kernel/list.h"
#include "../../libc/include/string.h"
#include "../../libc/include/kernel/tree.h"
#include "../../libc/include/fcntl.h"
#include "../terminal.h"

file_node_t* root_node;
tree_t* file_tree;

file_node_t* resolve_path(char* cwd, char* file, file_node_t** outParent, char** outFileName);

static int id_generator = 1;

int vfs_read_dir(struct FILE* node, struct list_dir* buffer, int count) {
    int i = 0;

    list_dir_t* ptr = buffer;

    tree_node_t* treeNode = tree_find_child_root(file_tree, node);

    if(!treeNode) {
        return 0;
    }

    for(list_entry_t* child = treeNode->children->head; child != NULL; child = child->next) {
        file_node_t* subNode = (file_node_t*)child->value;

        ptr->type = subNode->type;
        strncpy(ptr->name, subNode->name, strlen(subNode->name));
        ptr->size = subNode->size;

        ptr++;
        i++;

        if(i >= count) {
            break;
        }
    }

    return i;
}

bool vfs_create(struct FILE* parent, char* filename, int mode) {
    tree_node_t* treeNode = tree_find_child_root(file_tree, parent);

    if(!treeNode) {
        printf("Warning: Parent %s has no tree entry.\n", parent);
        return false;
    }

    file_node_t* node = calloc(1, sizeof(file_node_t));

    node->type = FILE_TYPE_FILE;
    node->size = 0;
    node->refcount = 0;
    node->id = id_generator++;
    strncpy(node->name, filename, strlen(filename));

    tree_insert_child(file_tree, treeNode, node);

    return true;
}

int mount_directly(char* name, file_node_t* node) {
    if(strlen(name) == 1 && memcmp(name, "/", strlen(name)) == 0) {
        //Root file system, just load directories
        if(root_node != null)  {
            //We just add the size for mapped directories.
            //Probably should do this different later
            //TODO: Proper cleanup on umount
            node->size += root_node->size;
        }

        root_node = node;

        file_tree->head->value = node;

        return 0;
    }

    file_node_t* parent = NULL;
    char* fileName = NULL;

    resolve_path("/", name, &parent, &fileName);

    if(parent == null) {
        return 1;
    }

    tree_node_t* treeNode = tree_find_child_root(file_tree, parent);

    if(treeNode == null) {
        return 2;
    }

    file_node_t* parentNode = (file_node_t*)treeNode->value;
    parentNode->size++;

    tree_insert_child(file_tree, treeNode, node);

    return 0;
}

int register_mount(char* name, mount_func func) {
    file_node_t* mount = func(name);

    return mount_directly(name, mount);
}

int mount_empty(char* name, int fileType) {
    file_node_t* node = calloc(1, sizeof(file_node_t));

    memset(&node->name, 0, 256);
    memcpy(&node->name, name, strlen(name));
    node->size = 0;
    node->type = fileType;
    node->id = 0; //Replace with get_next_id
    node->refcount = 0;

    return mount_directly(name, node);
}

tree_node_t* get_child(tree_node_t* parent, char* name) {
    for(list_entry_t* child = parent->children->head; child != null; child = child->next) {
        tree_node_t* node = (tree_node_t*)child->value;
        file_node_t* file = (file_node_t*)node->value;

        if(strcmp(name, file->name) == 0) {
            return node;
        }
    }

    return NULL;
}

tree_node_t* cache_node(tree_node_t* parent, file_node_t* node) {
    node->cached = true; //Mark as cached for drivers to ignore during counting.

    return tree_insert_child(file_tree, parent, node);
}

/***
 * Resolves the path from the current working directory.
 * The current working directory should always be already resolved!
 * @param cwd the current working directory
 * @param file the file to resolve
 * @param outParent the parent of the file if resolved
 * @param outFileName the name of the file
 * @return the resolved path
 */
file_node_t* resolve_path(char* cwd, char* file, file_node_t** outParent, char** outFileName) {
    tree_node_t* fCwd = NULL;
    file_node_t* fResult = NULL;

    if(!strcmp(file, "/")) {
        return root_node;
    }

    if(file[0] == '/') {
        fCwd = file_tree->head;
    } else {
        if(fCwd == NULL) {
            tree_node_t* current = file_tree->head;

            char* pch = (char *) NULL;
            char* save = NULL;
            printf("Resolving cwd: %s\n", cwd);

            pch = (char *) strtok_r(cwd, "/", &save);

            do {
                if(strcmp(pch, "..") == 0) {
                    //Funny.
                    if(current == file_tree->head) {
                        return root_node;
                    }

                    if(current->parent != NULL) {
                        current = current->parent;
                    }

                    pch = (char *) strtok_r(NULL, "/", &save);

                    continue;
                } else if(strcmp(pch, ".") == 0) {
                    pch = (char *) strtok_r(NULL, "/", &save);

                    continue;
                } else {
                    char* name = pch;
                    fCwd = current; // save temporarily in fCwd

                    current = get_child(current, pch);

                    if(current == NULL) {
                        //No node found, try fs
                        file_node_t* fs_node = fCwd->value;
                        if(fs_node->file_ops.find_dir) {
                            file_node_t* node = fs_node->file_ops.find_dir(fs_node, name);

                            if(node != NULL) {
                                //Add caching of node and continue traversal
                                current = cache_node(fCwd, node);

                                if(pch == NULL) {
                                    fCwd = current;
                                    break;
                                }

                                continue;
                            }
                        }

                        //We couldn't resolve cwd, wtf?
                        return NULL;
                    }
                }
            } while(pch != null);

            if(current != NULL) {
                fCwd = current;
            }
        }
    }

    tree_node_t* current = fCwd;
    tree_node_t* fParent = NULL;

    char* pch = (char *) NULL;
    char* save = NULL;
    printf("Resolving path: %s\n", file);

    pch = (char *) strtok_r(file, "/", &save);

    do {
        if(strcmp(pch, "..") == 0) {
            //Funny.
            if(current == file_tree->head) {
                return root_node;
            }

            if(current->parent != NULL) {
                current = current->parent;
            }

            pch = (char *) strtok_r(NULL, "/", &save);

            continue;
        } else if(strcmp(pch, ".") == 0) {
            pch = (char *) strtok_r(NULL, "/", &save);

            continue;
        } else {
            char* name = pch;

            fParent = current;

            current = get_child(current, name);

            pch = (char *) strtok_r(NULL, "/", &save);

            if(current == NULL) {
                //No node found, try fs
                file_node_t* fs_node = fParent->value;
                if(fs_node->file_ops.find_dir) {
                    file_node_t* node = fs_node->file_ops.find_dir(fs_node, name);

                    if(node != NULL) {
                        //Add caching of node and continue traversal
                        current = cache_node(fParent, node);

                        if(pch == NULL) {
                            //End of search, return current
                            return (file_node_t *) current->value;
                        }
                    }
                }

                if(pch == NULL) {
                    //Path is almost resolved, return parent for file creation
                    if(outParent != NULL) {
                        *outParent = fParent->value;
                    }

                    if(outFileName != NULL) {
                        *outFileName = strdup(name);
                    }
                }

                return NULL;
            }
        }
    } while(pch != null);

    if(current != NULL) {
        return (file_node_t*)current->value;
    }

    return NULL;
}

char* get_full_path(file_node_t* node) {
    list_t* list = list_create();

    list_insert(list, node->name);

    tree_node_t* treeNode = tree_find_child_root(file_tree, node);
    size_t pathSize = 0;

    if(treeNode == null) {
        return null;
    }

    for(tree_node_t* parent = treeNode->parent;; parent = parent->parent) {
        if(parent->parent == null) {
            break;
        }

        if(parent->value == root_node) {
            break;
        }

        pathSize += strlen(((file_node_t*)parent->value)->name) + 1;

        list_insert(list, ((file_node_t*)parent->value)->name);
    }

    char* path = malloc(pathSize * sizeof(char));
    char* ptr = path;
    memset(path, 0, pathSize * sizeof(char));

    for(list_entry_t * entry = list->tail; entry != null; entry = entry->prev) {
        char* name = entry->value;

        *ptr = '/';
        ptr++;

        strcpy(ptr, name);
        ptr += strlen(name);
    }

    list_free(list);

    return path;
}

file_node_t* open(char* filename, int mode) {
    if(strlen(filename) == 1 && memcmp(filename, "/", strlen(filename))) {
        return root_node;
    }

    file_node_t* node = resolve_path("/", filename, NULL, NULL);
    if(node->file_ops.open) {
        node->file_ops.open(node);
    }

    return node;
}

file_node_t* create(char* filename, int mode) {
    file_node_t* node = open(filename, mode);

    //If node exists, don't create it, just return invalid
    if(node != NULL) {
        return 0;
    }

    file_node_t* parent = NULL;
    char* outFileName = NULL;

    resolve_path("/", filename, &parent, &outFileName);

    parent->file_ops.create(parent, outFileName, mode);
    parent->size++;

    node = open(filename, mode);

    return node;
}

file_node_t* mkdir(char* dirname) {
    if(strlen(dirname) == 1 && memcmp(dirname, "/", strlen(dirname))) {
        return NULL;
    }

    file_node_t* node = open(dirname, 0);

    //If node exists, don't create it, just return invalid
    if(node != NULL) {
        return 0;
    }

    file_node_t* parent = NULL;
    char* filename = NULL;

    resolve_path("/", dirname, &parent, &filename);

    if(parent->file_ops.mkdir(parent, filename)) {
        file_node_t* node = open(filename, 0);

        return node;
    }

    return NULL;
}

file_node_t* mkdir_vfs(char* dirname) {
    if(strlen(dirname) == 1 && memcmp(dirname, "/", strlen(dirname))) {
        return NULL;
    }

    file_node_t* node = open(dirname, 0);

    //If node exists, don't create it, just return invalid
    if(node != NULL) {
        return 0;
    }

    file_node_t* parent = NULL;
    char* filename = NULL;

    resolve_path("/", dirname, &parent, &filename);

    tree_node_t* treeNode = tree_find_child_root(file_tree, parent);

    if(!treeNode) {
        printf("Warning: File %s is not present in file tree\n", parent->name);
        return NULL;
    }

    node = calloc(1, sizeof(file_node_t));

    node->type = FILE_TYPE_DIR; //Is set to mount point, because mount point is the most free to change type
    node->id = id_generator++; //id 1 will always be the root
    node->size = 0;
    strncpy(node->name, filename, strlen(dirname));
    node->refcount = 0;
    node->file_ops.read_dir = vfs_read_dir;
    node->file_ops.create = vfs_create;
    parent->size++;

    tree_insert_child(file_tree, treeNode, node);

    return node;
}

int getdents(file_node_t* node, list_dir_t** buffer, int count) {
    int i = 0; //Total count of entries received.

    list_dir_t* dir = calloc(count, sizeof(list_dir_t));

    tree_node_t* treeNode = tree_find_child_root(file_tree, node);

    if(!treeNode) {
        return 0;
    }

    i = node->file_ops.read_dir(node, dir, count);

    *buffer = dir;

    return i;
}

int get_size(file_node_t* node) {
    if(node->file_ops.get_size) {
        return node->file_ops.get_size(node);
    }

    return node->size;
}

int read(file_handle_t* handle, char* buffer, size_t length) {
    file_node_t* node = handle->fileNode;

    return node->file_ops.read(node, buffer, handle->offset, length);
}

int write(file_handle_t* handle, char* buffer, size_t length) {
    file_node_t* node = handle->fileNode;

    return node->file_ops.write(node, buffer, handle->offset, length);
}

/**
 * Utility function for the kernel to create in kernel handles.
 * The handle is volatile and should only be used to keep track inside the kernel.
 * Don't pass to user processes unless the handle is in the file descriptor table.
 * @return a file handle
 */
file_handle_t* create_handle(file_node_t* node) {
    file_handle_t* handle = calloc(1, sizeof(file_handle_t));
    handle->fileNode = node;
    handle->mode = 4; // 4 = FULL ACCESS; Kernel mode
    handle->offset = 0;

    return handle;
}

int get_next_file_id() {
    return id_generator++;
}

tree_t* debug_get_file_tree() {
    return file_tree;
}

struct file_operations fileOperations = {
    .create = vfs_create
};

struct file_operations* get_vfs_ops() {

}

void vfs_install() {
    printf("VFS INIT");
    file_tree = tree_create();

    root_node = calloc(1, sizeof(file_node_t));

    root_node->type = FILE_TYPE_MOUNT_POINT; //Is set to mount point, because mount point is the most free to change type
    root_node->id = id_generator++; //id 1 will always be the root
    root_node->size = 0;
    strncpy(root_node->name, "[root]", 6);
    root_node->refcount = 0;
    root_node->file_ops.read_dir = vfs_read_dir;

    tree_insert_child(file_tree, NULL, root_node);

    //At this point, drivers would read the root file system
    printf("VFS INIT END");
}