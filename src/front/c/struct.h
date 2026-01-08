#ifndef _STRUCT_H
#define _STRUCT_H

#include "type.h"

typedef struct struct_member_s
{
    type_t type;
    char *ident;
} struct_member_t;

LIST_DECL(struct_member_t, struct_member)

typedef struct struct_s
{
    list_struct_member_t members;
} struct_t;

LIST_DECL(struct_t, struct)

void struct_freemember(struct_member_t* member);
void struct_free(struct_t* structure);

#endif