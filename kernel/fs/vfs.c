//
// Created by Jannik on 07.04.2024.
//
#include "vfs.h"
#include "../proc/process.h"
#include <string.h>
#include "../../libc/include/kernel/tree.h"
#include "../../libc/include/fcntl.h"

file_node_t* root_node;
tree_t* file_tree;

static int id_generator = 1;

void vfs_listdir(file_node_t* dir, file_node_t** entries) {

}

file_node_t* vfs_open(char* filename) {

}

file_node_t* vfs_create(char* filename, int mode) {

}

file_node_t* vfs_mkdir(char* filename) {

}

int vfs_read(file_node_t* node, int offset, int len) {

}

const struct file_operations fileOperations = {
    .read = vfs_read,
    .read_dir = vfs_listdir,
    .open = vfs_open,
    .create = vfs_create,
    .mkdir = vfs_mkdir;
};

void mount_directly(char* name, file_node_t* node) {
    if(strlen(name) == 1 && memcmp(name, "/", strlen(name))) {
        //Root file system, just load directories
        root_node = node;

        tree_insert_child(tree, NULL, root_node);
    }

    //
}

void register_mount(char* name, mount_func func) {
    file_node_t* mount = func(name);

    mount_directly(name, mount);
}

void mount_empty(char* name) {
    file_node_t* node = calloc(1, sizeof(file_node_t));

    node->name = name;
    node->size = 0;
    node->type = FILE_TYPE_MOUNT_POINT;
    node->id = 0; //Replace with get_next_id
    node->refcount = 0;

    mount_directly(name, node);
}

tree_node_t* get_child(tree_node_t* parent, char* name) {
    for(list_node_t* child = parent->children->head; child != null; child = child->next) {
        tree_node_t* node = (tree_node_t*)child->value;
        file_node_t* file = (file_node_t*)node->value;

        if(strcmp(name, file->name) == 0) {
            return node;
        }
    }

    return NULL;
}

tree_node_t* cache_node(tree_node_t* parent, file_node_t* node) {
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

    if(file[0] == '/') {
        fCwd = file_tree->head;
    } else {
        if(fCwd == NULL) {
            tree_node_t* current = file_tree->head;

            char* pch = NULL;
            char* save = NULL;
            printf("Resolving cwd: %s", cwd);

            pch = strtok_r(cwd, "/", &save);

            do {
                if(strcmp(pch, "..") == 0) {
                    //Funny.
                    if(current == file_tree->head) {
                        return root_node;
                    }

                    if(current->parent != NULL) {
                        current = current->parent;
                    }

                    pch = strtok_r(NULL, "/", &save);

                    continue;
                } else if(strcmp(pch, ".") == 0) {
                    pch = strtok_r(NULL, "/", &save);

                    continue;
                } else {
                    fCwd = current; // save temporarily in fCwd

                    current = get_child(current, pch);

                    if(current == NULL) {
                        //No node found, try fs
                        file_node_t* fs_node = fCwd->value;
                        if(fs_node->file_ops->find_dir) {
                            file_node_t* node = fs_node->file_ops->find_dir(fs_node, name);

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

    char* pch = NULL;
    char* save = NULL;
    printf("Resolving path: %s", file);

    pch = strtok_r(file, "/", &save);

    do {
        if(strcmp(pch, "..") == 0) {
            //Funny.
            if(current == file_tree->head) {
                return root_node;
            }

            if(current->parent != NULL) {
                current = current->parent;
            }

            pch = strtok_r(NULL, "/", &save);

            continue;
        } else if(strcmp(pch, ".") == 0) {
            pch = strtok_r(NULL, "/", &save);

            continue;
        } else {
            char* name = pch;

            fParent = current;

            current = get_child(current, name);

            pch = strtok_r(NULL, "/", &save);

            if(current == NULL) {
                //No node found, try fs
                file_node_t* fs_node = fParent->value;
                if(fs_node->file_ops->read_dir) {
                    file_node_t* node = fs_node->file_ops->find_dir(fs_node, name);

                    if(node != NULL) {
                        //Add caching of node and continue traversal
                        current = cache_node(fParent, node);

                        if(pch == NULL) {
                            //End of search, return current
                            return current;
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
}

file_node_t* open(char* filename, int mode) {
    if(strlen(name) == 1 && memcmp(name, "/", strlen(name))) {
        return root_node;
    }

    file_node_t* node = resolve_path("/", filename, NULL, NULL);

    return node;
}

file_node_t* create(char* filename, int mode) {
    file_node_t* node = open(filename, mode);

    //If node exists, don't create it, just return invalid
    if(node != NULL) {
        return 0;
    }

    file_node_t* parent = NULL;
    char* filename = NULL;

    resolve_path("/", file, &parent, &filename);

    parent->file_ops->create(parent, filename, mode);

    file_node_t* node = open(filename, mode);

    return node;
}

file_node_t* mkdir(char* dirname) {
    if(strlen(name) == 1 && memcmp(name, "/", strlen(name))) {
        return NULL;
    }

    file_node_t* node = open(filename, 0);

    //If node exists, don't create it, just return invalid
    if(node != NULL) {
        return 0;
    }

    file_node_t* parent = NULL;
    char* filename = NULL;

    resolve_path("/", file, &parent, &filename);

    if(parent->file_ops->mkdir(parent, filename)) {
        file_node_t* node = open(filename, 0);

        return node;
    }

    return NULL;
}

/**
 * Utility function for the kernel to create in kernel handles.
 * The handle is volatile and should only be used to keep track inside the kernel.
 * Don't pass to user processes unless the handle is in the file descriptor table.
 * @return a file handle
 */
file_handle_t* create_handle(file_node_t* node) {
    file_handle_t handle = calloc(1, sizeof(file_handle_t));
    handle.fileNode = node;
    handle.mode = 4; // 4 = FULL ACCESS; Kernel mode
    handle.offset = 0;

    return handle;
}

int get_next_file_id() {
    return id_generator++;
}

void vfs_install() {
    file_tree = tree_create();

    root_node = calloc(1, sizeof(file_node_t));

    root_node.type = FILE_TYPE_MOUNT_POINT; //Is set to mount point, because mount point is the most free to change type
    root_node.id = id_generator++; //id 1 will always be the root
    root_node.size = 0;
    root_node.name = "[root]";
    root_node.refcount = 0;
    root_node.file_ops = fileOperations;

    tree_insert_child(tree, NULL, root_node);

    //At this point, drivers would read the root file system
}