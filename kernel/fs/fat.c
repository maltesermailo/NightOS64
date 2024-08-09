//
// Created by Jannik on 14.07.2024.
//
#include "fat.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

void clean_fat_filename(char* input, char* output) {
    char name[9] = {0};  // 8 chars for name + null terminator
    char ext[4] = {0};   // 3 chars for extension + null terminator
    int i, j;

    // Copy name (first 8 characters)
    for (i = 0; i < 8 && input[i] && input[i] != ' '; i++) {
        name[i] = input[i];
    }

    // Copy extension (last 3 characters)
    for (j = 0; j < 3 && input[8+j] && input[8+j] != ' '; j++) {
        ext[j] = input[8+j];
    }

    // Construct cleaned filename
    if (ext[0]) {
        snprintf(output, 12, "%s.%s", name, ext);
    } else {
        strcpy(output, name);
    }

    // Convert to lowercase
    for (i = 0; output[i]; i++) {
        if (output[i] >= 'A' && output[i] <= 'Z') {
            output[i] = output[i] + 32;  // Convert to lowercase
        }
    }
}


uint32_t fat_find_free_cluster(fat_fs_t *fs) {
    uint32_t *fat = malloc(fs->sectorSize);
    uint32_t fat_sector = fs->fatPointer;
    uint32_t entries_per_sector = fs->sectorSize / sizeof(uint32_t);

    for (uint32_t i = 2; i < fs->capacity / fs->clusterSize; i++) {
        if (i % entries_per_sector == 0) {
            // Read next FAT sector
            fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)fat,
                                              (fat_sector + i / entries_per_sector) * fs->sectorSize, fs->sectorSize);
        }

        if (fat[i % entries_per_sector] == 0) {
            free(fat);
            return i;
        }
    }

    free(fat);
    return 0; // No free cluster found
}

void fat_update_fat(fat_fs_t *fs, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4; //FAT uses 32-bit addressing with the upper 4 bits reserved
    uint32_t fat_sector = fs->fatPointer + (fat_offset / fs->sectorSize);
    uint32_t entry_offset = fat_offset % fs->sectorSize;

    uint32_t *fat_sector_data = malloc(fs->sectorSize);

    // Read FAT sector
    fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)fat_sector_data,
                                      fat_sector * fs->sectorSize, fs->sectorSize);

    // Update FAT entry
    fat_sector_data[entry_offset / 4] = value;

    // Write updated FAT sector back to disk
    fs->physicalDevice->file_ops.write(fs->physicalDevice, (char*)fat_sector_data,
                                       fat_sector * fs->sectorSize, fs->sectorSize);

    free(fat_sector_data);
}

int fat_create_dir_entry(fat_fs_t *fs, file_node_t *dir, const char *name, uint32_t cluster, uint32_t size, uint8_t attributes) {
    uint32_t dir_cluster = ((fat_entry_t*)dir->fs)->cluster;
    uint32_t cluster_size = fs->clusterSize * fs->sectorSize;
    uint32_t entries_per_cluster = cluster_size / sizeof(struct fatDirEntry);

    struct fatDirEntry *dir_data = malloc(cluster_size);

    // Read directory cluster
    fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)dir_data,
                                      fs->dataPointer * fs->sectorSize + (dir_cluster - 2) * cluster_size, cluster_size);

    // Find a free entry
    for (uint32_t i = 0; i < entries_per_cluster; i++) {
        if (dir_data[i].filename[0] == 0x00 || dir_data[i].filename[0] == 0xE5) {
            // Free entry found, create new entry
            memset(&dir_data[i], 0, sizeof(struct fatDirEntry));
            strncpy((char*)dir_data[i].filename, name, 11);
            dir_data[i].attributes = attributes;
            dir_data[i].clusterLow = cluster & 0xFFFF;
            dir_data[i].clusterHigh = (cluster >> 16) & 0xFFFF;
            dir_data[i].size = size;

            // Write updated directory cluster back to disk
            fs->physicalDevice->file_ops.write(fs->physicalDevice, (char*)dir_data,
                                               fs->dataPointer * fs->sectorSize + (dir_cluster - 2) * cluster_size, cluster_size);

            free(dir_data);
            return 0;
        }
    }

    free(dir_data);
    return -1; // No free entry found
}

uint32_t fat_get_next_cluster(fat_fs_t *fs, uint32_t cluster) {
    uint32_t fat_offset = cluster * 4; //FAT uses 32-bit addressing with the upper 4 bits reserved
    uint32_t fat_sector = fs->fatPointer + (fat_offset / fs->sectorSize);
    uint32_t entry_offset = fat_offset % fs->sectorSize;

    uint32_t *fat_sector_data = malloc(fs->sectorSize);

    // Read FAT sector
    fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)fat_sector_data,
                                      fat_sector * fs->sectorSize, fs->sectorSize);

    // Update FAT entry
    uint32_t value = fat_sector_data[entry_offset / 4] & 0x0FFFFFFF;

    free(fat_sector_data);

    return value;
}

int fat_write_file_data(fat_fs_t *fs, uint32_t start_cluster, const char *data, size_t size) {
    uint32_t cluster_size = fs->clusterSize * fs->sectorSize;
    uint32_t current_cluster = start_cluster;
    size_t bytes_written = 0;

    while (bytes_written < size) {
        size_t chunk_size = (size - bytes_written < cluster_size) ? size - bytes_written : cluster_size;

        // Write data to current cluster
        fs->physicalDevice->file_ops.write(fs->physicalDevice, data + bytes_written,
                                           fs->dataPointer * fs->sectorSize + (current_cluster - 2) * cluster_size, chunk_size);

        bytes_written += chunk_size;

        if (bytes_written < size) {
            // Need another cluster
            uint32_t next_cluster = fat_get_next_cluster(fs, start_cluster);

            if(next_cluster >= 0x0FFFFFFF) {
                next_cluster = fat_find_free_cluster(fs);
                if (next_cluster == 0) {
                    return -1; // No more free clusters
                }

                // Update FAT for current cluster
                fat_update_fat(fs, current_cluster, next_cluster);
            }

            current_cluster = next_cluster;
        }
    }

    // Mark end of file in FAT
    fat_update_fat(fs, current_cluster, 0x0FFFFFFF);

    return 0;
}

bool fat_create_file(struct FILE *dir, char *name, int mode) {
    fat_fs_t *fs = ((fat_entry_t*)dir->fs)->fatFs;

    // Find a free cluster for the new file
    uint32_t cluster = fat_find_free_cluster(fs);
    if (cluster == 0) {
        return false; // No free clusters
    }

    // Create directory entry
    if (fat_create_dir_entry(fs, dir, name, cluster, 0, 0) != 0) {
        return false; // Couldn't create directory entry
    }

    // Mark cluster as end of file in FAT
    fat_update_fat(fs, cluster, 0x0FFFFFFF);

    // Create and return new file node
    file_node_t *new_file = calloc(1, sizeof(file_node_t));
    strcpy(new_file->name, name);
    new_file->type = FILE_TYPE_FILE;
    new_file->size = 0;

    fat_entry_t *new_entry = calloc(1, sizeof(fat_entry_t));
    new_entry->fatFs = fs;
    new_entry->cluster = cluster;

    new_file->fs = new_entry;
    new_file->file_ops.read = fat_read;
    new_file->file_ops.write = fat_write;
    new_file->file_ops.get_size = fat_get_size;
    new_file->file_ops.delete = fat_delete;

    return true;
}

bool fat_mkdir(struct FILE *dir, char *name) {
    if(dir->type != FILE_TYPE_DIR && dir->type != FILE_TYPE_MOUNT_POINT) {
        return false;
    }

    fat_fs_t *fs = ((fat_entry_t*)dir->fs)->fatFs;

    // Find a free cluster for the new file
    uint32_t cluster = fat_find_free_cluster(fs);
    if (cluster == 0) {
        return false; // No free clusters
    }

    // Create directory entry
    if (fat_create_dir_entry(fs, dir, name, cluster, 0, 0x10) != 0) {
        return false; // Couldn't create directory entry
    }

    // Mark cluster as end of file in FAT
    fat_update_fat(fs, cluster, 0x0FFFFFFF);

    // Create and return new file node
    file_node_t *new_file = calloc(1, sizeof(file_node_t));
    strcpy(new_file->name, name);
    new_file->type = FILE_TYPE_DIR;
    new_file->size = 0;

    fat_entry_t *new_entry = calloc(1, sizeof(fat_entry_t));
    new_entry->fatFs = fs;
    new_entry->cluster = cluster;

    new_file->fs = new_entry;
    new_file->file_ops.read = fat_read;
    new_file->file_ops.write = fat_write;
    new_file->file_ops.get_size = fat_get_size;
    new_file->file_ops.mkdir = fat_mkdir;

    return true;
}

int fat_read_file_data(fat_fs_t *fs, uint32_t start_cluster, const char *data, size_t size) {
    uint32_t cluster_size = fs->clusterSize * fs->sectorSize;
    uint32_t current_cluster = start_cluster;
    size_t bytes_read = 0;

    while (bytes_read < size) {
        size_t chunk_size = (size - bytes_read < cluster_size) ? size - bytes_read : cluster_size;

        // Write data to current cluster
        fs->physicalDevice->file_ops.read(fs->physicalDevice, data + bytes_read,
                                           fs->dataPointer * fs->sectorSize + (current_cluster - 2) * cluster_size, chunk_size);

        bytes_read += chunk_size;

        if (bytes_read < size) {
            // Need another cluster
            uint32_t next_cluster = fat_get_next_cluster(fs, current_cluster);
            if (next_cluster >= 0x0FFFFFF8) {
                return bytes_read;
            }

            current_cluster = next_cluster;
        }
    }

    return bytes_read;
}

int fat_read(struct FILE *file, char *data, size_t size, size_t offset) {
    fat_entry_t *entry = (fat_entry_t*)file->fs;
    fat_fs_t *fs = entry->fatFs;

    uint32_t cluster_size = fs->clusterSize * fs->sectorSize;
    uint32_t start_cluster = entry->cluster;

    // Skip to the correct cluster for the offset
    while (offset >= cluster_size) {
        start_cluster = fat_get_next_cluster(fs, start_cluster);
        if (start_cluster >= 0x0FFFFFF8) {
            // End of file chain reached before offset
            return -1;
        }
        offset -= cluster_size;
    }

    // Write data
    int result = fat_read_file_data(fs, start_cluster, data, size);

    return result;
}

int fat_update_dir_entry_size(fat_fs_t *fs, file_node_t *file) {
    tree_node_t* treeNode = tree_find_child_root(debug_get_file_tree(), file);

    if(treeNode == null) {
        return -1;
    }

    tree_node_t* parentNode = treeNode->parent;

    if(parentNode == NULL) {
        return -1;
    }

    file_node_t* parent = (file_node_t*)parentNode->value;

    fat_entry_t *parent_entry = (fat_entry_t*)parent->fs;
    uint32_t parent_cluster = parent_entry->cluster;
    uint32_t cluster_size = fs->clusterSize * fs->sectorSize;

    struct fatDirEntry *dir_data = malloc(cluster_size);

    // Read parent directory cluster
    fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)dir_data,
                                      fs->dataPointer * fs->sectorSize + (parent_cluster - 2) * cluster_size, cluster_size);

    // Find the entry for this file
    for (uint32_t i = 0; i < cluster_size / sizeof(struct fatDirEntry); i++) {
        if (strncmp((char*)dir_data[i].filename, file->name, 11) == 0) {
            // Update file size
            dir_data[i].size = file->size;

            // Write updated directory cluster back to disk
            fs->physicalDevice->file_ops.write(fs->physicalDevice, (char*)dir_data,
                                               fs->dataPointer * fs->sectorSize + (parent_cluster - 2) * cluster_size, cluster_size);

            free(dir_data);
            return 0;
        }
    }

    free(dir_data);
    return -1; // Entry not found
}

int fat_write(struct FILE *file, char *data, size_t size, size_t offset) {
    fat_entry_t *entry = (fat_entry_t*)file->fs;
    fat_fs_t *fs = entry->fatFs;

    uint32_t cluster_size = fs->clusterSize * fs->sectorSize;
    uint32_t start_cluster = entry->cluster;

    // Skip to the correct cluster for the offset
    while (offset >= cluster_size) {
        uint32_t next_cluster = fat_get_next_cluster(fs, start_cluster);
        if (next_cluster >= 0x0FFFFFF8) {
            // End of file chain reached
            if (offset > file->size) {
                // Trying to write beyond current EOF
                // Allocate new cluster(s) as needed
                next_cluster = fat_find_free_cluster(fs);
                if (next_cluster == 0) {
                    return -1; // No more free clusters
                }

                char* nullBuffer = malloc(cluster_size);
                memset(nullBuffer, 0, cluster_size);

                fat_write_file_data(fs, start_cluster, nullBuffer, cluster_size);

                free(nullBuffer);

                fat_update_fat(fs, start_cluster, next_cluster);
                fat_update_fat(fs, next_cluster, 0x0FFFFFFF); // Mark as EOF
            } else {
                return -1; // Invalid offset
            }
        }
        start_cluster = next_cluster;
        offset -= cluster_size;
    }

    // Write data
    int result = fat_write_file_data(fs, start_cluster, data, size);
    if (result == 0) {
        // Update file size in directory entry

        if(offset > size) {
            file->size = offset + size;
        } else {
            file->size = file->size - offset + size;
        }

        fat_update_dir_entry_size(fs, file);
    }

    return result;
}

int fat_delete(struct FILE* file) {
    fat_entry_t *entry = (fat_entry_t*)file->fs;
    fat_fs_t *fs = entry->fatFs;

    uint32_t start_cluster = entry->cluster;

    if(file->type == FILE_TYPE_FILE) {
        uint32_t current_cluster = start_cluster;

        while (current_cluster < 0x0FFFFFF7) {
            uint32_t next_cluster = fat_get_next_cluster(fs, current_cluster);

            // Update FAT for current cluster
            fat_update_fat(fs, current_cluster, 0);

            current_cluster = next_cluster;
        }
    } else if(file->type == FILE_TYPE_DIR) {
        if(fat_get_size(file) > 0) {
            return -2; //NOT EMPTY
        }

        uint32_t current_cluster = start_cluster;

        while (current_cluster < 0x0FFFFFF7) {
            uint32_t next_cluster = fat_get_next_cluster(fs, current_cluster);

            // Update FAT for current cluster
            fat_update_fat(fs, current_cluster, 0);

            current_cluster = next_cluster;
        }
    }

    return 0;
}

file_node_t* fat_find_dir(file_node_t* node, char* name) {
    if(node->type != FILE_TYPE_DIR && node->type != FILE_TYPE_MOUNT_POINT) {
        return NULL;
    }

    if(node->type == FILE_TYPE_MOUNT_POINT) {
        fat_fs_t* fatDirectory = (fat_fs_t*)node->fs;
        uint32_t clusterByteSize = fatDirectory->clusterSize * fatDirectory->sectorSize;
        uint64_t startPointer = fatDirectory->dataPointer * fatDirectory->sectorSize + (fatDirectory->fatEbr32.root_cluster - 2) * clusterByteSize;

        struct fatDirEntry* fatDirPointer = calloc(1, clusterByteSize);

        fatDirectory->physicalDevice->file_ops.read(fatDirectory->physicalDevice, (char*)fatDirPointer, startPointer, clusterByteSize);

        int i = 0;

        while(1) {
            //Read dir entries
            if(fatDirPointer->filename[0] == 0x00) {
                //End of files
                break;
            }

            if(fatDirPointer->filename[0] == 0xE5) {
                //Reserved entry
                fatDirPointer++;
                continue;
            }

            if(fatDirPointer->attributes == 0x0F) {
                printf("WARNING: Long file names not supported by driver.\n");
                fatDirPointer++;
                continue;
            }

            char filename[12];
            memset(&filename, 0, 12);
            clean_fat_filename((char*)&fatDirPointer->filename, (char*)&filename);

            if(strcmp(filename, name) == 0) {
                file_node_t* newNode = calloc(1, sizeof(file_node_t));
                clean_fat_filename(newNode->name, filename);

                newNode->type = fatDirPointer->attributes & 0x10 ? FILE_TYPE_DIR : FILE_TYPE_FILE;
                newNode->size = fatDirPointer->size;

                fat_entry_t* newEntry = calloc(1, sizeof(fat_entry_t));
                newEntry->fatFs = node->fs;
                newEntry->cluster = (uint32_t)fatDirPointer->clusterHigh << 16 | fatDirPointer->clusterLow;

                newNode->fs = newEntry;

                //TODO: FILE OPS
                newNode->file_ops.find_dir = fat_find_dir;
                newNode->file_ops.read = fat_read;
                newNode->file_ops.write = fat_write;
                newNode->file_ops.get_size = fat_get_size;
                newNode->file_ops.mkdir = fat_mkdir;
                newNode->file_ops.delete = fat_delete;
                newNode->file_ops.create = fat_create_file;
                return newNode;
            }
        }

        free(fatDirPointer);

        return NULL;
    }

    fat_entry_t* fatDirectory = (fat_entry_t*)node->fs;
    uint32_t clusterByteSize = fatDirectory->fatFs->clusterSize * fatDirectory->fatFs->sectorSize;
    uint64_t startPointer = fatDirectory->fatFs->dataPointer + (fatDirectory->cluster - 2) * clusterByteSize;

    struct fatDirEntry* fatDirPointer = calloc(1, clusterByteSize);

    fatDirectory->fatFs->physicalDevice->file_ops.read(fatDirectory->fatFs->physicalDevice, (char*)fatDirPointer, startPointer, clusterByteSize);

    while(1) {
        //Read dir entries
        if(fatDirPointer->filename[0] == 0x00) {
            //End of files
            break;
        }

        if(fatDirPointer->filename[0] == 0xE5) {
            //Reserved entry
            fatDirPointer++;
            continue;
        }

        if(fatDirPointer->attributes == 0x0F) {
            printf("WARNING: Long file names not supported by driver.\n");
            fatDirPointer++;
            continue;
        }

        char filename[12];
        memset(&filename, 0, 12);
        clean_fat_filename((char*)&fatDirPointer->filename, (char*)&filename);

        if(strcmp(filename, name) == 0) {
            file_node_t* newNode = calloc(1, sizeof(file_node_t));
            strcpy(newNode->name, fatDirPointer->filename);

            newNode->type = fatDirPointer->attributes & 0x10 ? FILE_TYPE_DIR : FILE_TYPE_FILE;
            newNode->size = fatDirPointer->size;

            fat_entry_t* newEntry = calloc(1, sizeof(fat_entry_t));
            newEntry->fatFs = node->fs;
            newEntry->cluster = (uint32_t)fatDirPointer->clusterHigh << 16 | fatDirPointer->clusterLow;

            newNode->fs = newEntry;

            //TODO: FILE OPS
            newNode->file_ops.find_dir = fat_find_dir;
            newNode->file_ops.read = fat_read;
            newNode->file_ops.write = fat_write;
            newNode->file_ops.get_size = fat_get_size;
            newNode->file_ops.mkdir = fat_mkdir;
            newNode->file_ops.delete = fat_delete;
            newNode->file_ops.create = fat_create_file;
            return newNode;
        }
    }

    free(fatDirPointer);

    return NULL;
}

int fat_read_dir(file_node_t* node, list_dir_t* entries, int count) {
    if(node->type != FILE_TYPE_DIR && node->type != FILE_TYPE_MOUNT_POINT) {
        return 0;
    }

    if(node->type == FILE_TYPE_MOUNT_POINT) {
        fat_fs_t* fatDirectory = (fat_fs_t*)node->fs;
        uint32_t clusterByteSize = fatDirectory->clusterSize * fatDirectory->sectorSize;
        uint64_t startPointer = fatDirectory->dataPointer * fatDirectory->sectorSize + (fatDirectory->fatEbr32.root_cluster - 2) * clusterByteSize;

        struct fatDirEntry* fatDirPointer = calloc(1, clusterByteSize);

        fatDirectory->physicalDevice->file_ops.read(fatDirectory->physicalDevice, (char*)fatDirPointer, startPointer, clusterByteSize);

        int i = 0;

        while(i < count) {
            //Read dir entries
            if(fatDirPointer->filename[0] == 0x00) {
                //End of files
                break;
            }

            if(fatDirPointer->filename[0] == 0xE5) {
                //Reserved entry
                fatDirPointer++;
                continue;
            }

            if(fatDirPointer->attributes == 0x0F) {
                printf("WARNING: Long file names not supported by driver.\n");
                fatDirPointer++;
                continue;
            }

            clean_fat_filename((char*)&fatDirPointer->filename, (char*)&entries[i].name);

            entries[i].type = fatDirPointer->attributes & 0x10 ? FILE_TYPE_DIR : FILE_TYPE_FILE;
            entries[i].size = fatDirPointer->size;

            fatDirPointer++;
            i++;
        }

        free(fatDirPointer);

        return i;
    }

    fat_entry_t* fatDirectory = (fat_entry_t*)node->fs;
    uint32_t clusterByteSize = fatDirectory->fatFs->clusterSize * fatDirectory->fatFs->sectorSize;
    uint64_t startPointer = fatDirectory->fatFs->dataPointer * fatDirectory->fatFs->sectorSize + (fatDirectory->cluster - 2) * clusterByteSize;

    struct fatDirEntry* fatDirPointer = calloc(1, clusterByteSize);

    fatDirectory->fatFs->physicalDevice->file_ops.read(fatDirectory->fatFs->physicalDevice, (char*)fatDirPointer, startPointer, clusterByteSize);

    int i = 0;

    while(i < count) {
        //Read dir entries
        if(fatDirPointer->filename[0] == 0x00) {
            //End of files
            break;
        }

        if(fatDirPointer->filename[0] == 0xE5) {
            //Reserved entry
            fatDirPointer++;
            continue;
        }

        if(fatDirPointer->attributes == 0x0F) {
            printf("WARNING: Long file names not supported by driver.\n");
            fatDirPointer++;
            continue;
        }

        clean_fat_filename((char*)&fatDirPointer->filename, (char*)&entries[i].name);

        entries[i].type = fatDirPointer->attributes & 0x10 ? FILE_TYPE_DIR : FILE_TYPE_FILE;
        entries[i].size = fatDirPointer->size;
        fatDirPointer++;
        i++;
    }

    free(fatDirPointer);

    return i;
}

int fat_get_size(file_node_t* node) {
    if(node->type != FILE_TYPE_DIR && node->type != FILE_TYPE_MOUNT_POINT) {
        return node->size;
    }

    if(node->type == FILE_TYPE_MOUNT_POINT) {
        fat_fs_t* fatDirectory = (fat_fs_t*)node->fs;
        uint32_t clusterByteSize = fatDirectory->clusterSize * fatDirectory->sectorSize;
        uint64_t startPointer = fatDirectory->dataPointer * fatDirectory->sectorSize + (fatDirectory->fatEbr32.root_cluster - 2) * clusterByteSize;

        struct fatDirEntry* fatDirPointer = calloc(1, clusterByteSize);

        fatDirectory->physicalDevice->file_ops.read(fatDirectory->physicalDevice, (char*)fatDirPointer, startPointer, clusterByteSize);

        int count = 0;

        while(1) {
            //Read dir entries
            if(fatDirPointer->filename[0] == 0x00) {
                //End of files
                break;
            }

            if(fatDirPointer->filename[0] == 0xE5) {
                //Reserved entry
                fatDirPointer++;
                continue;
            }

            if(fatDirPointer->attributes == 0x0F) {
                printf("WARNING: Long file names not supported by driver.\n");
                fatDirPointer++;
                continue;
            }

            fatDirPointer++;
            count++;
        }

        free(fatDirPointer);
        return count;
    }

    fat_entry_t* fatDirectory = (fat_entry_t*)node->fs;
    uint32_t clusterByteSize = fatDirectory->fatFs->clusterSize * fatDirectory->fatFs->sectorSize;
    uint64_t startPointer = fatDirectory->fatFs->dataPointer * fatDirectory->fatFs->sectorSize + (fatDirectory->cluster - 2) * clusterByteSize;

    struct fatDirEntry* fatDirPointer = calloc(1, clusterByteSize);

    fatDirectory->fatFs->physicalDevice->file_ops.read(fatDirectory->fatFs->physicalDevice, (char*)fatDirPointer, startPointer, clusterByteSize);

    int count = 0;

    while(1) {
        //Read dir entries
        if(fatDirPointer->filename[0] == 0x00) {
            //End of files
            break;
        }

        if(fatDirPointer->filename[0] == 0xE5) {
            //Reserved entry
            fatDirPointer++;
            continue;
        }

        if(fatDirPointer->attributes == 0x0F) {
            printf("WARNING: Long file names not supported by driver.\n");
            fatDirPointer++;
            continue;
        }

        fatDirPointer++;
        count++;
    }

    free(fatDirPointer);

    return count;
}

file_node_t* fat_mount(char* device, char* name) {
    file_node_t* deviceNode = open(device, 0);

    if(!deviceNode) {
        printf("FAT: No device node\n");
        return NULL;
    }

    if(deviceNode->type != FILE_TYPE_BLOCK_DEVICE) {
        printf("FAT: No block device.\n");
        return NULL;
    }

    fat_fs_t* fatFs = calloc(1, sizeof(fat_fs_t));
    deviceNode->file_ops.read(deviceNode, (char*)&fatFs->fatBpb, 0, 512);

    mbr_t* mbr = calloc(1, sizeof(mbr_t));
    memcpy(mbr, &fatFs->fatBpb, 512);

    if(mbr->signature == MBR_SIGNATURE) {
        printf("MBR Partition Table detected. Reading...\n");

        for(int i = 0; i < 4; i++) {
            partition_entry_t* partitionEntry = &mbr->partitionEntries[i];

            //Check if active
            if(partitionEntry->systemId == 0x0C) {
                //Found FAT32 partition
                fatFs->baseOffset = (unsigned int)partitionEntry->startLBA;

                deviceNode->file_ops.read(deviceNode, (char*)&fatFs->fatBpb, fatFs->baseOffset * 512, 512);
            }
        }
    }

    if(fatFs->fatEbr32.boot_signature != 0x28 && fatFs->fatEbr32.boot_signature != 0x29) {
        printf("FAT: Wrong boot signature.\n");
        free(fatFs);

        return NULL;
    }

    printf("FAT: Number of bytes per sector %d\n", fatFs->fatBpb.bytes_per_sector);
    printf("FAT: Sectors per cluster: %d\n", fatFs->fatBpb.sectors_per_cluster);
    printf("FAT: Number of FATs: %d\n", fatFs->fatBpb.table_count);
    printf("FAT: Number of root dir entries: %d\n", fatFs->fatBpb.root_entry_count);
    printf("FAT: Sector count 16: %d\n", fatFs->fatBpb.total_sectors_16);
    printf("FAT: Extended sector count: %d\n", fatFs->fatBpb.total_sectors_32);

    fatFs->fatPointer = fatFs->fatBpb.reserved_sector_count + fatFs->baseOffset;
    fatFs->sectorSize = fatFs->fatBpb.bytes_per_sector;
    fatFs->clusterSize = fatFs->fatBpb.sectors_per_cluster;
    fatFs->capacity = fatFs->fatBpb.total_sectors_32;
    fatFs->dataPointer = fatFs->fatPointer + fatFs->fatBpb.table_count * fatFs->fatEbr32.table_size_32; //This is the location of the first sector
    fatFs->physicalDevice = deviceNode;

    char* dirEntryBytes = calloc(1, fatFs->sectorSize * fatFs->clusterSize);
    struct fatDirEntry* fatDirPointer = (struct fatDirEntry*) dirEntryBytes;

    //read root cluster
    deviceNode->file_ops.read(deviceNode, dirEntryBytes, (fatFs->dataPointer * fatFs->sectorSize) + fatFs->sectorSize * fatFs->clusterSize * (fatFs->fatEbr32.root_cluster - 2), fatFs->sectorSize * fatFs->clusterSize);

    while(1) {
        //Read dir entries
        if(fatDirPointer->filename[0] == 0x00) {
            //End of files
            break;
        }

        if(fatDirPointer->filename[0] == 0xE5) {
            //Reserved entry
            fatDirPointer++;
            continue;
        }

        if(fatDirPointer->attributes == 0x0F) {
            printf("WARNING: Long file names not supported by driver.\n");
            fatDirPointer++;
            continue;
        }

        char filename[12];
        memset(&filename, 0, 12);
        clean_fat_filename((char*)&fatDirPointer->filename, (char*)&filename);

        printf("File name: %s\n", filename);
        printf("File type: %d\n", fatDirPointer->attributes);
        printf("Cluster: %d\n", (uint64_t)((uint64_t)fatDirPointer->clusterHigh << 32) | fatDirPointer->clusterLow);

        fatDirPointer++;

        if((uintptr_t)(dirEntryBytes + fatFs->sectorSize * fatFs->clusterSize) <= (uintptr_t)fatDirPointer) {
            break;
        }
    }

    //TODO: Make utility function to create file_nodes
    file_node_t* fatNode = calloc(1, sizeof(file_node_t));
    fatNode->type = FILE_TYPE_MOUNT_POINT;
    fatNode->fs = fatFs;
    fatNode->size = 0;
    char* last_slash = strrchr(name, '/') + 1;
    strcpy(fatNode->name, last_slash);
    fatNode->file_ops.read_dir = fat_read_dir;
    fatNode->file_ops.find_dir = fat_find_dir;
    fatNode->file_ops.get_size = fat_get_size;

    fatFs->rootPointer = fatNode;

    return fatNode;
}