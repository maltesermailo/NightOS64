//
// Created by Jannik on 14.10.2024.
//

#ifndef NIGHTOS_CACHE_H
#define NIGHTOS_CACHE_H

#include "vfs.h"

#define CACHE_MAX_SIZE 1024 * 1024 * 64  // 64 MB cache size
#define CACHE_BLOCK_SIZE 4096            // 4 KB cache block size

typedef struct cache_entry {
  file_node_t* file;
  uint64_t offset;
  uint8_t* data;
  uint64_t size;
  bool dirty;
} vfs_cache_entry_t;

typedef struct cache {
  uint64_t total_size;

  list_t entries;
} vfs_cache_t;

void vfs_cache_init(void);
int vfs_cache_read(file_node_t* file, char* buffer, size_t offset, size_t size);
int vfs_cache_write(file_node_t* file, const char* buffer, size_t offset, size_t size);
void vfs_cache_flush(file_node_t* file);

#endif // NIGHTOS_CACHE_H
