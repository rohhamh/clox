#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = 0;
}

void free_table(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

static Entry* find_entry(Entry* entries, int capacity, Value* key) {
    uint32_t index;
    switch (key->type) {
        case VAL_BOOL:
            if (AS_BOOL(*key) == true) index = 1;
            else index = capacity - 1;
            break;
        case VAL_NIL:
            index = 0;
            break;
        case VAL_NUMBER:
            index = (int)AS_NUMBER(*key) % capacity;
            break;
        case VAL_OBJ:
            if (IS_STRING(*key)) {
                index = (AS_STRING(*key))->hash % capacity;
            }
            break;
    }
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool table_get(Table* table, Value* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjust_capacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(Table* table, Value* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool table_delete(Table* table, Value* key) {
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_add_all(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set(to, entry->key, entry->value);
        }
    }
}

ObjString* table_find_string(Table* table, const char* chars,
                             int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) return NULL;
        } else switch (entry->key->type) {
            case VAL_OBJ:
                if (IS_STRING(*entry->key)) {
                    ObjString* key = AS_STRING(*entry->key);
                    if (key->length == length &&
                        key->hash == hash &&
                        memcmp(key->chars, chars, length) == 0) {
                        return key;
                    }
                }
                break;
            default:
                printf("table_find_string called for entry that's not a string");
                break;
        }

        index = (index + 1) % table->capacity;
    }
}
