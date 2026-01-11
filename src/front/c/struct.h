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
MAP_DECL(char*, struct_t*, str, pstruct)
LIST_DECL(map_str_pstruct_t, map_str_pstruct)

// pointing to NULL means its been declared but not defined
extern list_map_str_pstruct_t structtags;

void struct_init(void);

void struct_enterscope(void);
void struct_exitscope(void);
void struct_processtype(type_t* type, bool indirect);
struct_t* struct_finddef(type_t* type);
struct_t* struct_findtagdef(char* tag);
void struct_cpy(struct_t* dst, struct_t* src);
void struct_cpymember(struct_member_t* dst, struct_member_t* src);

void struct_freemember(struct_member_t* member);
void struct_free(struct_t* structure);

#endif