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

    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}