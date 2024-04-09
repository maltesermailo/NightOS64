//
// Created by Jannik on 08.04.2024.
//

#ifndef NIGHTOS_LIST_H
#define NIGHTOS_LIST_H

typedef struct list_entry {
    struct list_entry* prev;
    struct list_entry* next;
    void* value;
} __attribute__((packed)) list_entry_t;

typedef struct {
    list_entry_t * head;
    list_entry_t * tail;

    size_t length;
} __attribute__((packed)) list_t;

list_t* list_create();
void list_destroy(list_t* list); //Frees everything including the value
void list_free(list_t* list); // Removes all entries and frees them as well as the list
void list_append(list_t* list, list_entry_t* entry);
void list_insert(list_t* list, void* item);
list_entry_t* list_find(list_t* list, void* item);
void list_remove_by_index(list_t* list, size_t index);
void list_delete(list_t* list, list_entry_t* entry);
list_t* list_copy(list_t* original);
void list_append_after(list_t* list, list_entry_t* before, list_entry_t* after);
list_entry_t* list_insert_after(list_t* list, list_entry_t* before, void* item);

void list_append_before(list_t* list, list_entry_t* after, list_entry_t* before);
list_entry_t* list_insert_before(list_t* list, list_entry_t* after, void* item);

#endif //NIGHTOS_LIST_H
