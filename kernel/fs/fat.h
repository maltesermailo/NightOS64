//
// Created by Jannik on 14.07.2024.
//

#ifndef NIGHTOS_FAT_H
#define NIGHTOS_FAT_H

#include <stdint.h>
#include "vfs.h"

#define FAT_ATTRIB_READ_ONLY 0x01
#define FAT_ATTRIB_HIDDEN    0x02
#define FAT_ATTRIB_SYSTEM    0x04
#define FAT_ATTRIB_VOLUME_ID 0x08
#define FAT_ATTRIB_DIRECTORY 0x10
#define FAT_ATTRIB_ARCHIVE   0x20
#define FAT_ATTRIB_LFN       FAT_ATTRIB_READ_ONLY | FAT_ATTRIB_HIDDEN | FAT_ATTRIB_SYSTEM | FAT_ATTRIB_VOLUME_ID

typedef struct fatBPB
{
    unsigned char 		bootjmp[3];
    unsigned char 		oem_name[8];
    unsigned short 	    bytes_per_sector;
    unsigned char		sectors_per_cluster;
    unsigned short		reserved_sector_count;
    unsigned char		table_count;
    unsigned short		root_entry_count;
    unsigned short		total_sectors_16;
    unsigned char		media_type;
    unsigned short		table_size_16;
    unsigned short		sectors_per_track;
    unsigned short		head_side_count;
    unsigned int 		hidden_sector_count;
    unsigned int 		total_sectors_32;

}__attribute__((packed)) fatBPB_t;

typedef struct fatEBR32
{
    //extended fat32 stuff
    unsigned int		table_size_32;
    unsigned short		extended_flags;
    unsigned short		fat_version;
    unsigned int		root_cluster;
    unsigned short		fat_info;
    unsigned short		backup_BS_sector;
    unsigned char 		reserved_0[12];
    unsigned char		drive_number;
    unsigned char 		reserved_1;
    unsigned char		boot_signature;
    unsigned int 		volume_id;
    unsigned char		volume_label[11];
    unsigned char		fat_type_label[8];
    unsigned char       boot_code[420];
    unsigned short      boot_partition_signature;
}__attribute__((packed)) fatEBR32_t;

typedef struct fatFSInfo {
    unsigned int        lead_signature;
    unsigned char       reserved0[480];
    unsigned int        signature;
    unsigned int        lastFreeCluster;
    unsigned int        startFreeCluster;
    unsigned int        reserved1;
    unsigned int        reserved2;
    unsigned int        reserved3;
    unsigned int        trail_signature;
}__attribute__((packed)) fatFSInfo_t;

typedef struct fatDirEntry {
    unsigned char filename[11];
    unsigned char attributes;
    unsigned char windowsNT;
    unsigned char creationTimeHundreds;
    unsigned short creationTime;
    unsigned short creationDate;
    unsigned short lastAccessedDate;
    unsigned short clusterHigh;
    unsigned short lastModificationTime;
    unsigned short lastModificationDate;
    unsigned short clusterLow;
    unsigned int size;
};

typedef struct PartitionEntry {
    unsigned char bootFlag;
    unsigned char startHead;
    unsigned short startSector : 6;
    unsigned short startCylinder: 10;
    unsigned char systemId;
    unsigned char endingHead;
    unsigned short endingSector : 6;
    unsigned short endingCylinder: 10;
    unsigned int startLBA;
    unsigned int totalSectors;
} __attribute__((packed)) partition_entry_t;

#define MBR_SIGNATURE 0xAA55

typedef struct MBR {
    unsigned char boot_code[440];
    unsigned int driveId;
    unsigned short res0;
    partition_entry_t partitionEntries[4];
    unsigned short signature;
} mbr_t;

typedef struct FatFilesystem {
    fatBPB_t fatBpb;
    fatEBR32_t fatEbr32;
    fatFSInfo_t fatFsInfo;

    uint32_t baseOffset;

    uint32_t sectorSize; //In bytes
    uint32_t clusterSize;

    uint32_t capacity; //In sectors

    uint64_t fatPointer; //Pointer to the fat in sectors
    uint64_t dataPointer; //First data sector

    file_node_t* physicalDevice;
    file_node_t* rootPointer;
} fat_fs_t;

typedef struct FatEntry {
    fat_fs_t* fatFs;

    uint32_t cluster;
} fat_entry_t;

file_node_t* fat_mount(char* device, char* name);
bool fat_create_file(file_node_t *dir, char *name, int mode);
int fat_read(file_node_t *file, char *data, size_t size, size_t offset);
int fat_write(file_node_t *file, char *data, size_t size, size_t offset);
int fat_get_size(file_node_t* node);
int fat_delete(struct FILE* file);

#endif //NIGHTOS_FAT_H
