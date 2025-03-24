#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
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

static ObjString* allocate_string(char* chars, int length) {
    ObjString *string = ALLOCATE_OBJ_STRING(length + 1);
    string->length = length;
    strcpy(string->chars, chars);
    string->chars[length] = '\0';
    return string;
}

ObjString* take_string(char* chars, int length) {
    return allocate_string(chars, length);
}

ObjString* copy_string(const char* chars, int length) {
    char* heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(heap_chars, length);
}

void print_object(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}
