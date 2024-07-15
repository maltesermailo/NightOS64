//
// Created by Jannik on 15.07.2024.
//
#include <string.h>
#include <stdlib.h>
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