#ifndef STRUCT_H
#define STRUCT_H

#include "compiler.h"

int struct_register(const char *name);
int struct_find_by_name(const char *name);
int struct_member_index(int struct_id, const char *member_name);
int struct_member_offset(int struct_id, const char *member_name);
int struct_get_size(int struct_id);
int struct_get_alignment(int struct_id);
int type_size(int t);

#endif
