//
// Created by Jannik on 08.10.2024.
//


#ifndef NIGHTOS_ISOFS_H
#define NIGHTOS_ISOFS_H

#include <stdint.h>
#include "vfs.h"

#define VOLUME_TYPE_BOOT          0
#define VOLUME_TYPE_PRIMARY       1
#define VOLUME_TYPE_SUPPLEMENTARY 2
#define VOLUME_TYPE_PARTITION     3
#define VOLUME_TYP_TERMINATOR     255

#define DIRECTORY_RECORD_SIZE 255

#define DIRECTORY_FLAG_HIDDEN     1 << 0
#define DIRECTORY_FLAG_DIR        1 << 1
#define DIRECTORY_FLAG_ASSOCIATED 1 << 2
#define DIRECTORY_FLAG_EXT_FORMAT 1 << 3
#define DIRECTORY_FLAG_EXT_PERMS  1 << 4
#define DIRECTORY_FLAG_NOT_FINAL  1 << 7

typedef struct DirectoryRecord {
  uint8_t length;
  uint8_t extendedAttributeLength;
  uint32_t lba;
  uint32_t lba_MSB;
  uint32_t data_length;
  uint32_t data_length_MSB;

  struct {
    uint8_t years;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t offsetFromGMTIn15Minutes;
  }  __attribute__((packed)) date;

  uint8_t flags;
  uint8_t file_size_interleaved;
  uint8_t interleave_gap_size;
  uint16_t volume_sequence_number;
  uint16_t volume_sequence_number_MSB;
  uint8_t file_name_length;

  union {
    char filename[];
    char additionalData[];
  };
}  __attribute__((packed)) iso_directory_t;

typedef struct PathTable {
  uint8_t length;
  uint8_t extendedAttributeLength;
  uint32_t lba;
  uint16_t parent;
  char filename[];
} __attribute__((packed)) path_table_t;

typedef struct VolumeDescriptor {
    uint8_t type;
    char identifier[5];
    uint8_t version;

    union {
        struct {
            char boot_system_identifier[32];
            char boot_identifier[32];
            char boot_system_use[1977];
        } __attribute__((packed)) boot_record;
        struct {
            char system_identifier[32];
            char volume_identifier[32];
            uint64_t unused;
            uint32_t volume_space_size;
            uint32_t volume_space_size_MSB;
            char unused_2[32];
            uint16_t volume_set_size;
            uint16_t volume_set_size_MSB;
            uint16_t volume_sequence_number;
            uint16_t volume_sequence_number_MSB;
            uint16_t logical_block_size;
            uint16_t logical_block_size_MSB;
            uint32_t path_table_size;
            uint32_t path_table_size_MSB;
            uint32_t type_l_path_table;
            uint32_t opt_type_l_path_table;
            uint32_t type_m_path_table;
            uint32_t opt_type_m_path_table;
            char root_directory_record[34];
            char volume_set_identifier[128];
            char publisher_identifier[128];
            char data_preparer_identifier[128];
            char application_identifier[128];
            char copyright_file_identifier[37];
            char abstract_file_identifier[37];
            char bibliographic_file_identifier[37];
            char volume_creation_date_time[17];
            char volume_modification_date_time[17];
            char volume_expiration_date_time[17];
            char volume_effective_date_time[17];
            uint8_t file_structure_version;
            uint8_t reserved;
            char application_use[512];
            char reserved_2[653];
        } __attribute__((packed)) primary_volume;
        char data[2041];
    };
} __attribute__((packed)) volume_descriptor_t;

typedef struct IsoFilesystem {
  volume_descriptor_t* volumeDescriptor;

  file_node_t* physicalDevice;
  iso_directory_t* root;
} iso_fs_t;

typedef struct IsoEntry {
  iso_fs_t* isoFs;

  iso_directory_t* entry;
} iso_entry_t;

int parseFromAscii(char* ascii, int len);
file_node_t* isofs_mount(char* device, char* name);
file_node_t* isofs_find_dir(file_node_t* node, char* name);
int isofs_read_dir(file_node_t* node, list_dir_t* entries, int count);
int isofs_get_size(file_node_t* node);
int isofs_read(struct FILE *file, char *data, size_t size, size_t offset);

#endif //NIGHTOS_ISOFS_H
