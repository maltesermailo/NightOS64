//
// Created by Jannik on 14.10.2024.
//

#include "cache.h"
#include "../alloc.h"
#include <string.h>
#include "../terminal.h"
#include "../../mlibc/abis/linux/errno.h"

vfs_cache_t* cache;

//TODO: Rework to use error codes
static void cache_remove_entry(int i, vfs_cache_entry_t* entry) {
  list_remove_by_index(&cache->entries, i);

  cache->total_size -= entry->size;
}

static void cache_add_entry(vfs_cache_entry_t* entry) {
  list_insert(&cache->entries, entry);

  cache->total_size += entry->size;
}

static void cache_evict(bool flush) {
  while ((cache->total_size > CACHE_MAX_SIZE || flush) && cache->entries.head) {
    vfs_cache_entry_t* to_evict = (vfs_cache_entry_t*) cache->entries.head;
    cache_remove_entry(0, to_evict);

    if (to_evict->dirty) {
      // Write back dirty data to the actual file
      to_evict->file->file_ops.write(to_evict->file, (char*)to_evict->data, to_evict->offset, to_evict->size);
    }

    free(to_evict->data);
    kfree(to_evict);
  }
}

static vfs_cache_entry_t* cache_find(file_node_t* file, size_t offset, size_t size, bool is_write) {
  for (list_entry_t* list_entry = cache->entries.head; list_entry != NULL; list_entry = list_entry->next) {
    vfs_cache_entry_t* entry = (vfs_cache_entry_t*) list_entry->value;
    if (entry->file == file &&
        entry->offset <= offset && ((entry->offset + entry->size) > offset)) {
      if(!is_write && (entry->offset + entry->size) > entry->file->size) {
        if(entry->offset > entry->file->size) {
          size = 0;

          return NULL;
        } else {
          size = (entry->file->size - entry->offset);
        }
      }

      if(entry->offset + entry->size < offset + size) {
        uint8_t* old_data = entry->data;
        uint64_t old_size = entry->size;

        int64_t size_variance = ((offset + size) - (entry->offset + entry->size));

        if(size_variance > 0) {
          uint8_t* new_data = calloc(1, entry->size + size_variance);

          memcpy(new_data, old_data, entry->size);
          entry->data = new_data;
          entry->size = entry->size + size_variance;
          cache->total_size += size_variance;
          free(old_data);

          entry->file->file_ops.read(file, (char*)entry->data + old_size, old_size, size - old_size);
          cache_evict(false);
        }
      }

      return entry;
    }
  }
  return NULL;
}

void vfs_cache_init(void) {
  alloc_register_object_size(sizeof(vfs_cache_entry_t));

  cache = kcalloc(1, sizeof(vfs_cache_t));
  cache->total_size = 0;
}

int vfs_cache_read(file_node_t* file, char* buffer, size_t offset, size_t size) {
  vfs_cache_entry_t* entry = cache_find(file, offset, size, false);

  printf("VFS: Cache: read\n");

  if (entry) {
    // Cache hit
    printf("VFS: Cache: hit\n");

    //If the segment request is larger than our entry, decrease requested size
    if((offset - entry->offset) + size > entry->size) {
      size = entry->size - (offset - entry->offset);
    }

    memcpy(buffer, entry->data + (offset - entry->offset), size);
    return size;
  }

  printf("VFS: Cache: miss\n");
  // Cache miss
  entry = kmalloc(sizeof(vfs_cache_entry_t));

  if(!entry) {
    return -ENOMEM;
  }

  entry->file = file;
  entry->offset = offset;
  entry->size = size;
  entry->data = calloc(1, size);
  entry->dirty = false;

  int read = file->file_ops.read(file, (char*)entry->data, offset, size);
  if (read > 0) {
    memcpy(buffer, entry->data, read);
    cache_add_entry(entry);
    cache_evict(false);
  } else {
    free(entry->data);
    kfree(entry);
  }

  return read;
}

int vfs_cache_write(file_node_t* file, const char* buffer, size_t offset, size_t size) {
  vfs_cache_entry_t* entry = cache_find(file, offset, size, true);

  if (entry) {
    // Cache hit, update existing entry
    memcpy(entry->data + (offset - entry->offset), buffer, size);
    entry->dirty = true;
  } else {
    // Cache miss, create new entry
    entry = kmalloc(sizeof(vfs_cache_entry_t));

    if(!entry) {
      return -ENOMEM;
    }

    entry->file = file;
    entry->offset = offset;
    entry->size = size;
    entry->data = calloc(1, size);
    memcpy(entry->data, buffer, size);
    entry->dirty = true;
    cache_add_entry(entry);
  }

  cache_evict(false);
  return size;
}

void vfs_cache_flush(file_node_t* file) {
  for (list_entry_t* list_entry = cache->entries.head; list_entry != NULL; list_entry = list_entry->next) {
    vfs_cache_entry_t* entry = (vfs_cache_entry_t*) list_entry->value;
    if (entry->file == file &&
        entry->dirty) {
      file->file_ops.write(file, (char*)entry->data, entry->offset, entry->size);
      entry->dirty = false;
    }
  }
}