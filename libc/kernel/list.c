//
// Created by Jannik on 08.04.2024.
//
#include "../include/kernel/list.h"
#include "../../kernel/alloc.h"
#include "../include/stdio.h"

list_t * list_create() {
    list_t* list = calloc(1, sizeof(list_t));

    return list;
}

void list_destroy(list_t* list) {
    list_entry_t* listEntry = list->head;
    list->head = NULL;

    while(listEntry) {
        list_entry_t* listEntryOld = listEntry;
        free(listEntry->value);

        listEntry = listEntry->next;

        free(listEntryOld);
    }

    free(list);
}

void list_free(list_t* list) {
    list_entry_t* listEntry = list->head;
    list->head = NULL;

    while(listEntry) {
        list_entry_t* listEntryOld = listEntry;

        listEntry = listEntry->next;

        free(listEntryOld);
    }

    free(list);
}

void list_append(list_t* list, list_entry_t* entry) {
    if(!list->length) {
        list->head = entry;
        list->tail = entry;
        list->length = 1;

        return;
    }

    list->tail->next = entry;
    entry->prev = list->tail;
    list->tail = entry;

    list->length++;
}

void list_insert(list_t* list, void* item) {
    list_entry_t * entry = calloc(1, sizeof(list_entry_t));
    entry->value = item;

    list_append(list, entry);
}

list_entry_t* list_find(list_t* list, void* item) {
    if(!list->length) {
        //List empty
        return 0;
    }

    list_entry_t* entry = list->head;

    for(;entry != 0;) {
        if(entry->value == item) {
            return entry;
        }

        entry = entry->next;
    }

    //No entry
    return 0;
}

void list_remove_by_index(list_t* list, size_t index) {
    if(!list->length) {
        return;
    }

    if(index == list->length) {
        list_delete(list, list->tail);
    }

    list_entry_t* entry = list->head;

    if(index == 0) {
        list_delete(list, entry);
    } else {
        for(size_t i = 1; i < index; i++) {
            if(!entry->next) {
                return;
            }

            entry = entry->next;
        }

        list_delete(list, entry);
    }
}

void list_delete(list_t* list, list_entry_t* entry) {
    list_entry_t* prev = entry->prev;
    list_entry_t* next = entry->next;

    if(list->head == entry) {
        list->head = next; //If list size is 1, next will be 0 anyways
    }

    if(list->tail == entry) {
        list->tail = prev; //Same as above
    }

    if(next && prev) {
        //if both exist, link together
        next->prev = prev;
        prev->next = next;
    } else {
        //if one exists, remove their link to this entry
        if(next) {
            next->prev = 0;
        }

        if(prev) {
            prev->next = 0;
        }
    }

    entry->next = 0;
    entry->prev = 0;

    free(entry);

    list->length--;
}

list_t* list_copy(list_t* original) {
    list_t* new = calloc(1, sizeof(list_t));

    for(list_entry_t* entry = original->head; entry != NULL; entry = entry->next) {
        list_entry_t* copy = calloc(1, sizeof(list_entry_t));
        copy->value = entry->value;

        list_append(new, copy);
    }

    return new;
}

void list_append_after(list_t* list, list_entry_t* before, list_entry_t* after) {
    if(before->next) {
        after->next = before->next; //Set our appended next entry to next entry of the preceeding one
        before->next = after; // Point preceeding entry to new entry

        after->prev = before; // Point our prev entry to preceeding entry
        after->next->prev = after; // Point next entry prev pointer to us

        list->length++;
    } else {
        //before is tail, so just add
        list_append(list, after);
    }
}

list_entry_t* list_insert_after(list_t* list, list_entry_t* before, void* item) {
    list_entry_t* entry = calloc(1, sizeof(list_entry_t));
    entry->value = item;

    list_append_after(list, before, entry);

    return entry;
}

void list_append_before(list_t* list, list_entry_t* after, list_entry_t* before) {
    if(after->prev) {
        before->prev = after->prev;
        before->next = after;

        after->prev = before;
        before->prev->next = before;

        list->length++;
    } else {
        list->head = before;
        after->prev = before;

        before->next = after;

        list->length++;
    }
}

list_entry_t* list_insert_before(list_t* list, list_entry_t* after, void* item) {
    list_entry_t* entry = calloc(1, sizeof(list_entry_t));
    entry->value = item;

    list_append_before(list, after, entry);

    return entry;
}

void list_dump(list_t* list) {
    int i = 0;

    printf("List pointer is 0x%x\n", list);

    for(list_entry_t* entry = list->head; entry != NULL; entry = entry->next) {
        printf("List entry %d references to 0x%x, next is 0x%x\n", i, entry->value, entry->next);
        i++;

        if(i > list->length) {
            printf("Open-ended list, can't continue!!!");
            break;
        }
    }

    if(i > list->length) {
        printf("List length, doesn't correspond to read index. Read index: %d, List lenght: %d\n", i, list->length);
    }

    if(!list->head) {
        printf("No list head\n");
    }

    if(!list->tail) {
        printf("No list tail\n");
    }
}