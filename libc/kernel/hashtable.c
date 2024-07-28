//
// Created by Jannik on 15.07.2024.
//
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/kernel/hashtable.h"

unsigned int hash(const char* key, int table_size) {
    unsigned long int value = 0;
    unsigned int i = 0;
    unsigned int key_len = strlen(key);

    // Do several rounds of multiplication
    for (; i < key_len; ++i) {
        value = value * 37 + key[i];
    }

    // Make sure value is 0 <= value < table_size
    value = value % table_size;

    return value;
}

struct hashtable* ht_create(int size) {
    struct hashtable* table = malloc(sizeof(struct hashtable));
    table->size = size;
    table->entries = calloc(size, sizeof(struct ht_entry*));
    return table;
}

void ht_insert(struct hashtable* table, const char* key, void* value) {
    unsigned int slot = hash(key, table->size);
    struct ht_entry* entry = table->entries[slot];

    if (entry == NULL) {
        table->entries[slot] = malloc(sizeof(struct ht_entry));
        table->entries[slot]->key = strdup(key);
        table->entries[slot]->value = value;
        table->entries[slot]->next = NULL;
    } else {
        while (entry->next != NULL) {
            if (strcmp(entry->key, key) == 0) {
                entry->value = value;
                return;
            }
            entry = entry->next;
        }

        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
        } else {
            entry->next = malloc(sizeof(struct ht_entry));
            entry->next->key = strdup(key);
            entry->next->value = value;
            entry->next->next = NULL;
        }
    }
}

void* ht_lookup(struct hashtable* table, const char* key) {
    unsigned int slot = hash(key, table->size);
    struct ht_entry* entry = table->entries[slot];

    if (entry == NULL) {
        return NULL;
    }

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

void ht_delete(struct hashtable* table, const char* key) {
    unsigned int slot = hash(key, table->size);
    struct ht_entry* entry = table->entries[slot];
    struct ht_entry* prev = NULL;

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            if (prev == NULL) {
                // Deleting the first entry in the slot
                table->entries[slot] = entry->next;
            } else {
                prev->next = entry->next;
            }
            free(entry->key);
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void ht_free(struct hashtable* table) {
    for (int i = 0; i < table->size; i++) {
        struct ht_entry* entry = table->entries[i];
        while (entry != NULL) {
            struct ht_entry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(table->entries);
    free(table);
}

bool ht_remove_by_key_and_value(struct hashtable* table, const char* key, void* value) {
    unsigned int slot = hash(key, table->size);
    struct ht_entry** entry = &(table->entries[slot]);

    while (*entry != NULL) {
        if (strcmp((*entry)->key, key) == 0 && (*entry)->value == value) {
            // Found matching key and value, remove this entry
            struct ht_entry* to_remove = *entry;
            *entry = (*entry)->next;
            free(to_remove->key);
            free(to_remove);
            return true;  // Entry removed successfully
        }
        entry = &((*entry)->next);
    }

    return false;  // Key-value pair not found
}

value_list* ht_get_all_values(struct hashtable* table, const char* key) {
    unsigned int slot = hash(key, table->size);
    struct ht_entry* entry = table->entries[slot];

    // First, count the number of matching entries
    int count = 0;
    struct ht_entry* current = entry;
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            count++;
        }
        current = current->next;
    }

    if (count == 0) {
        return NULL;  // No matching entries found
    }

    // Allocate the value_list structure and the array of value pointers
    value_list* result = malloc(sizeof(value_list));
    if (result == NULL) {
        return NULL;  // Memory allocation failed
    }
    result->values = malloc(count * sizeof(void*));
    if (result->values == NULL) {
        free(result);
        return NULL;  // Memory allocation failed
    }
    result->count = count;

    // Fill the array with the matching values
    int index = 0;
    current = entry;
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            result->values[index++] = current->value;
        }
        current = current->next;
    }

    return result;
}

// Don't forget to provide a function to free the value_list
void free_value_list(value_list* list) {
    if (list != NULL) {
        free(list->values);
        free(list);
    }
}