//
// Created by Jannik on 15.07.2024.
//

#ifndef NIGHTOS_HASHTABLE_H
#define NIGHTOS_HASHTABLE_H
#include <stdbool.h>

struct ht_entry {
    char* key;
    void* value;
    struct ht_entry* next;  // For handling collisions
};

struct hashtable {
    int size;
    struct ht_entry** entries;
};

// Create a new hash table
// size: The initial size of the hash table
// Returns: A pointer to the new hash table, or NULL if creation failed
struct hashtable* ht_create(int size);

// Insert a key-value pair into the hash table
// table: The hash table to insert into
// key: The key to insert
// value: The value to associate with the key
void ht_insert(struct hashtable* table, const char* key, void* value);

// Look up a key in the hash table
// table: The hash table to search in
// key: The key to look up
// Returns: The value associated with the key, or NULL if the key was not found
void* ht_lookup(struct hashtable* table, const char* key);

// Delete a key-value pair from the hash table
// table: The hash table to delete from
// key: The key to delete
void ht_delete(struct hashtable* table, const char* key);

// Free all memory associated with the hash table
// table: The hash table to free
void ht_free(struct hashtable* table);

bool ht_remove_by_key_and_value(struct hashtable* table, const char* key, void* value);

typedef struct {
    void** values;
    int count;
} value_list;

value_list* ht_get_all_values(struct hashtable* table, const char* key);
void free_value_list(value_list* list);

#endif //NIGHTOS_HASHTABLE_H
