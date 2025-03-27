#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include "object.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

#define ALLOCATE_OBJ_STRING(length) \
    (ObjString*)allocate_object(sizeof(ObjString) + (length) * sizeof(char), OBJ_STRING)

static Obj* allocate_object(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static ObjString* allocate_string(const char* chars, int length, uint32_t hash) {
    ObjString *string = ALLOCATE_OBJ_STRING(length + 1);
    string->length = length;
    strncpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->hash = hash;
    table_set(&vm.strings, &OBJ_VAL(string), NIL_VAL);
    return string;
}

static uint32_t hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint32_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* take_string(char* chars, int length) {
    uint32_t hash = hash_string(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    ObjString* string = allocate_string(chars, length, hash);
    string->is_owned = true;
    return string;
}

ObjString* copy_string(const char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    /*char* heap_chars = ALLOCATE(char, length + 1);*/
    /*memcpy(heap_chars, chars, length);*/
    /*heap_chars[length] = '\0';*/
    /*return allocate_string(heap_chars, length);*/
    ObjString* string = allocate_string(chars, length, hash);
    string->is_owned = false;
    return string;
}

void print_object(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}
