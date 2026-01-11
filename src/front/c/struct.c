#include "struct.h"

#include "front/error.h"

list_map_str_pstruct_t structtags;

void struct_freemember(struct_member_t* member)
{
    type_free(&member->type);
    free(member->ident);
}

void struct_free(struct_t* structure)
{
    list_struct_member_free(&structure->members);
}

void struct_init(void)
{
    list_map_str_pstruct_init(&structtags, 0);
}

void struct_enterscope(void)
{
    map_str_pstruct_t scope;

    map_str_pstruct_alloc(&scope);
    list_map_str_pstruct_ppush(&structtags, &scope);
}

void struct_exitscope(void)
{
    list_map_str_pstruct_pop(&structtags);
}

static void struct_define(char* tag, struct_t* def)
{
    assert(structtags.len);

    map_str_pstruct_set(&structtags.data[structtags.len-1], tag, def);
}

void struct_processtype(type_t* type, bool indirect)
{
    int i;

    if(type->type != TYPE_PTR && type->type != TYPE_STRUCT)
        return;

    if(type->type == TYPE_PTR)
        struct_processtype(type->ptrtype, true);

    // tag but no def
    if(type->struc.tag && !type->struc.def && !struct_findtagdef(type->struc.tag) && !indirect)
        error(true, type->line, type->column, "use of incomplete struct '%s'\n", type->struc.tag);

    if(type->struc.def)
        for(i=0; i<type->struc.def->members.len; i++)
            struct_processtype(&type->struc.def->members.data[i].type, false);

    // defining a tagged struct
    if(type->struc.tag && type->struc.def)
    {
        if(struct_findtagdef(type->struc.tag))
            error(true, type->line, type->column, "redefinition of '%s'\n", type->struc.tag);
        struct_define(type->struc.tag, type->struc.def);
    }
}

struct_t* struct_finddef(type_t* type)
{
    assert(type->type == TYPE_STRUCT);

    if(type->struc.def)
        return type->struc.def;

    assert(type->struc.tag);
    return struct_findtagdef(type->struc.tag);
}

struct_t* struct_findtagdef(char* tag)
{
    int i;

    struct_t **def;

    for(i=structtags.len-1; i>=0; i--)
    {
        def = map_str_pstruct_get(&structtags.data[i], tag);
        if(!def)
            continue;
        return *def;
    }

    return NULL;
}

void struct_cpy(struct_t* dst, struct_t* src)
{
    int i;

    list_struct_member_init(&dst->members, src->members.len);
    for(i=0; i<dst->members.len; i++)
        struct_cpymember(&dst->members.data[i], &src->members.data[i]);
}

void struct_cpymember(struct_member_t* dst, struct_member_t* src)
{
    type_cpy(&dst->type, &src->type);
    dst->ident = strdup(src->ident);
}

LIST_DEF(struct_member)
LIST_DEF_FREE_DECONSTRUCT(struct_member, struct_freemember)
MAP_DEF(char*, struct_t*, str, pstruct, hash_str, map_strcmp, map_strcpy, NULL, map_freestr, NULL)
LIST_DEF(map_str_pstruct)
LIST_DEF_FREE_DECONSTRUCT(map_str_pstruct, map_str_pstruct_free)