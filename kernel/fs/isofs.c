//
// Created by Jannik on 11.10.2024.
//
#include <string.h>
#include "isofs.h"
#include "../terminal.h"

int isofs_get_size(file_node_t* node) {
  if(node->type != FILE_TYPE_DIR && node->type != FILE_TYPE_MOUNT_POINT) {
    return node->size;
  }

  if(node->type == FILE_TYPE_MOUNT_POINT) {
    iso_fs_t* fs = (iso_fs_t*)node->fs;

    iso_directory_t* isoDirPointer = calloc(1, 255);

    int currentPos = fs->root->lba * fs->volumeDescriptor->primary_volume.logical_block_size;

    fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)isoDirPointer, fs->root->lba * 2048, 255);

    int count = 0;

    while(1) {
      //Read dir entries
      if(!(isoDirPointer->flags & DIRECTORY_FLAG_NOT_FINAL)) {
        //End of files
        break;
      }

      fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)isoDirPointer, currentPos, 255);

      currentPos += isoDirPointer->length;
      count++;
    }

    free(isoDirPointer);
    return count;
  }

  iso_entry_t* entry = (iso_entry_t*)node->fs;
  iso_fs_t* fs = entry->isoFs;

  iso_directory_t* isoDirPointer = calloc(1, 255);

  int currentPos = entry->entry->lba * fs->volumeDescriptor->primary_volume.logical_block_size;

  fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)isoDirPointer, currentPos, 255);

  int count = 0;

  while(1) {
    //Read dir entries
    if(!(isoDirPointer->flags & DIRECTORY_FLAG_NOT_FINAL)) {
      //End of files
      break;
    }

    fs->physicalDevice->file_ops.read(fs->physicalDevice, (char*)isoDirPointer, currentPos, 255);

    currentPos += isoDirPointer->length;
    count++;
  }

  free(isoDirPointer);
  return count;
}

file_node_t* isofs_mount(char* device, char* name) {
  file_node_t* deviceNode = open(device, 0);

  if(!deviceNode) {
    printf("FAT: No device node\n");
    return NULL;
  }

  if(deviceNode->type != FILE_TYPE_BLOCK_DEVICE) {
    printf("FAT: No block device.\n");
    return NULL;
  }

  file_node_t* root = NULL;

  int currentPos = 16 * 2048;

  volume_descriptor_t* volume = calloc(1, 2048);

  while(true) {
    int read = deviceNode->file_ops.read(deviceNode, (char*)&volume, currentPos, 2048);

    //Malformatted disk, we abort
    if(read < 2048) {
      break;
    }

    if(volume->type == 1) {
      //We found Primary volume descriptor, copy it and initialise fs
      volume_descriptor_t* primary = calloc(1, 2048);
      memcpy(primary, volume, 2048);

      iso_fs_t* fs = calloc(1, sizeof(iso_fs_t));
      iso_directory_t* rootDir = (iso_directory_t* )primary->primary_volume.root_directory_record;

      fs->root = rootDir;
      fs->volumeDescriptor = primary;

      root = calloc(1, sizeof(file_node_t));

      root->fs = fs;
      root->type = FILE_TYPE_MOUNT_POINT;
      root->size = 0;
      root->id = 0;

      char* last_slash = strrchr(name, '/') + 1;

      if(last_slash > name + strlen(name)) {
        //Might be root

        if(strcmp("/", name) == 0) {
          last_slash = name;
        }
      }

      strncpy(root->name, last_slash, strlen(last_slash));

      root->ref_count = 0;

      //These 3 functions are mostly needed for the root directory.
      root->file_ops.find_dir = NULL;
      root->file_ops.read_dir = NULL;
      root->file_ops.get_size = NULL;
    }

    //We didn't find a primary selector, so we abort
    if(volume->type == 255) {
      free(volume);
      break;
    }

    currentPos += 2048;
  }

  return root;
}