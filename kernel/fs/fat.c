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
            clean_fat_filename(&fatDirPointer->filename, &filename);

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
        clean_fat_filename(&fatDirPointer->filename, &filename);

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

            file_node_t* newNode = calloc(1, sizeof(file_node_t));
            clean_fat_filename(&fatDirPointer->filename ,&entries[i].name);

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

        file_node_t* newNode = calloc(1, sizeof(file_node_t));
        clean_fat_filename(&fatDirPointer->filename ,&entries[i].name);

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
        clean_fat_filename(&fatDirPointer->filename, &filename);

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