//
// Created by Jannik on 14.10.2024.
//

#include "cache.h"
#include "../alloc.h"
#include <string.h>

vfs_cache_t* cache;

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
    vfs_cache_entry_t* to_evict = cache->entries.head;
    cache_remove_entry(0, to_evict);

    if (to_evict->dirty) {
      // Write back dirty data to the actual file
      to_evict->file->file_ops.write(to_evict->file, (char*)to_evict->data, to_evict->offset, to_evict->size);
    }

    kfree(to_evict->data);
    kfree(to_evict);
  }
}

static vfs_cache_entry_t* cache_find(file_node_t* file, size_t offset, size_t size) {
  for (list_entry_t* list_entry = cache->entries.head; list_entry != NULL; list_entry = list_entry->next) {
    vfs_cache_entry_t* entry = (vfs_cache_entry_t*) list_entry->value;
    if (entry->file == file &&
        entry->offset <= offset) {
      if(entry->offset + entry->size < offset + size) {
        uint8_t* old_data = entry->data;

        int64_t size_variance = ((entry->offset + entry->size) - (offset + size));

        if(size_variance > 0) {
          uint8_t* new_data = kcalloc(1, entry->size + size_variance);

          memcpy(new_data, old_data, entry->size);
          entry->data = new_data;
          entry->size = entry->size + size_variance;
          kfree(old_data);
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
  vfs_cache_entry_t* entry = cache_find(file, offset, size);

  if (entry) {
    // Cache hit
    memcpy(buffer, entry->data + (offset - entry->offset), size);
    return size;
  }

  // Cache miss
  entry = kmalloc(sizeof(vfs_cache_entry_t));
  entry->file = file;
  entry->offset = offset;
  entry->size = size;
  entry->data = kmalloc(size);
  entry->dirty = false;

  int read = file->file_ops.read(file, (char*)entry->data, offset, size);
  if (read > 0) {
    memcpy(buffer, entry->data, read);
    cache_add_entry(entry);
    cache_evict(false);
  } else {
    kfree(entry->data);
    kfree(entry);
  }

  return read;
}

int vfs_cache_write(file_node_t* file, const char* buffer, size_t offset, size_t size) {
  vfs_cache_entry_t* entry = cache_find(file, offset, size);

  if (entry) {
    // Cache hit, update existing entry
    memcpy(entry->data + (offset - entry->offset), buffer, size);
    entry->dirty = true;
  } else {
    // Cache miss, create new entry
    entry = kmalloc(sizeof(vfs_cache_entry_t));
    entry->file = file;
    entry->offset = offset;
    entry->size = size;
    entry->data = kmalloc(size);
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