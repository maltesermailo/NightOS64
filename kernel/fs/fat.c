//
// Created by Jannik on 14.07.2024.
//
#include "fat.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

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

    fatFs->fatPointer = fatFs->fatBpb.reserved_sector_count;
    fatFs->sectorSize = fatFs->fatBpb.bytes_per_sector;
    fatFs->clusterSize = fatFs->fatBpb.sectors_per_cluster;
    fatFs->capacity = fatFs->fatBpb.total_sectors_32;
    fatFs->dataPointer = fatFs->fatPointer + fatFs->fatBpb.table_count * fatFs->fatEbr32.table_size_32; //This is the location of the first sector

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
            continue;
        }

        if(fatDirPointer->attributes == 0x0F) {
            printf("WARNING: Long file names not supported by driver.");
            continue;
        }

        printf("File name: %s\n", fatDirPointer->filename);
        printf("File type: %d\n", fatDirPointer->attributes);
        printf("Cluster: %d\n", (uint64_t)((uint64_t)fatDirPointer->clusterHigh << 32) | fatDirPointer->clusterLow);
    }

    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}