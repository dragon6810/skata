#include "struct.h"

#include "front/error.h"
#include "tag.h"

void struct_free(struct_t* struc)
{
    list_struct_member_free(&struc->members);
}

void struct_freemember(struct_member_t* member)
{
    type_free(&member->type);
    free(member->ident);
}

struct_t* struct_finddef(type_t* type)
{
    assert(type->type == TYPE_STRUCT);

    if(type->struc.tag)
    {
        if(type->struc.tag->defined)
            return &type->struc.tag->struc;
        return NULL;
    }

    if(type->struc.def)
        return type->struc.def;

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

static uint64_t struct_hashmix(uint64_t hash, uint64_t val)
{
    int i;

    for(i=0; i<8; i++, val>>=8)
        hash = (hash ^ (val & 0xFF)) * 0x100000001b3ULL;
    return hash;
}

static uint64_t struct_hashtype(uint64_t hash, type_t* type)
{
    struct_t *def;

    hash = struct_hashmix(hash, type->type);

    if(type->type != TYPE_STRUCT)
        return hash;

    def = type->struc.tag ? &type->struc.tag->struc : type->struc.def;
    struct_hashstruct(def);
    return struct_hashmix(hash, def->hash);
}

void struct_hashstruct(struct_t* struc)
{
    int i;

    struc->hash = TYPE_STRUCT;
    for(i=0; i<struc->members.len; i++)
        struc->hash = struct_hashtype(struc->hash, &struc->members.data[i].type);
}

LIST_DEF(struct_member)
LIST_DEF_FREE_DECONSTRUCT(struct_member, struct_freemember)
MAP_DEF(char*, struct_t*, str, pstruct, hash_str, map_strcmp, map_strcpy, NULL, map_freestr, NULL)
LIST_DEF(map_str_pstruct)
LIST_DEF_FREE_DECONSTRUCT(map_str_pstruct, map_str_pstruct_free)