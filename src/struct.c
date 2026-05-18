#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StructType struct_table[MAX_STRUCTS];
int struct_count = 0;

int struct_register(const char *name) {
    if (struct_count >= MAX_STRUCTS) {
        fprintf(stderr, "Error: demasiados structs\n");
        exit(1);
    }
    int id = struct_count++;
    strncpy(struct_table[id].name, name, MAX_LEXEME - 1);
    struct_table[id].name[MAX_LEXEME - 1] = '\0';
    struct_table[id].size = 0;
    struct_table[id].alignment = 0;
    struct_table[id].member_count = 0;
    return id;
}

int struct_find_by_name(const char *name) {
    for (int i = 0; i < struct_count; i++)
        if (!strcmp(struct_table[i].name, name))
            return i;
    return -1;
}

int struct_member_index(int struct_id, const char *member_name) {
    StructType *st = &struct_table[struct_id];
    for (int i = 0; i < st->member_count; i++)
        if (!strcmp(st->members[i].name, member_name))
            return i;
    return -1;
}

int struct_member_offset(int struct_id, const char *member_name) {
    int i = struct_member_index(struct_id, member_name);
    return (i >= 0) ? struct_table[struct_id].members[i].offset : -1;
}

int struct_get_size(int struct_id) {
    return struct_table[struct_id].size;
}

int struct_get_alignment(int struct_id) {
    return struct_table[struct_id].alignment;
}

int type_size(int t) {
    if (TYPE_IS_STRUCT(t)) return struct_get_size(TYPE_STRUCT_ID(t));
    if (TYPE_IS_PTR(t)) return 8;
    switch (t) {
        case TYPE_CHAR:   return 1;
        case TYPE_SHORT:  return 2;
        case TYPE_INT:    return 4;
        case TYPE_FLOAT:  return 4;
        case TYPE_DOUBLE: return 8;
        default:          return 8;
    }
}
