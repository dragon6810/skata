#include "type.h"

#include <stdio.h>

#include "ast.h"
#include "struct.h"

MAP_DEF(char*, type_t*, str, ptype, hash_str, map_strcmp, map_strcpy, NULL, map_freestr, NULL)

const char* type_names[TYPE_COUNT] =
{
    "void",
    "u1",
    "i8",
    "u8",
    "i16",
    "u16",
    "i32",
    "u32",
    "i64",
    "u64",
    "ptr",
    "func",
    "struct",
};

ir_primitive_e type_toprim(type_e type)
{
    switch(type)
    {
    case TYPE_U1:
        return IR_PRIM_U1;
    case TYPE_I8:
        return IR_PRIM_I8;
    case TYPE_U8:
        return IR_PRIM_U8;
    case TYPE_I16:
        return IR_PRIM_I16;
    case TYPE_U16:
        return IR_PRIM_U16;
    case TYPE_I32:
        return IR_PRIM_I32;
    case TYPE_U32:
        return IR_PRIM_U32;
    case TYPE_I64:
        return IR_PRIM_I64;
    case TYPE_U64:
        return IR_PRIM_U64;
    case TYPE_PTR:
        return IR_PRIM_PTR;
    case TYPE_FUNC:
        return IR_PRIM_PTR;
    default:
        printf("attempting to convert non-primitive type %d to an IR primitive!\n", (int) type);
        assert(0);
    }
}

int type_bytesize(type_t type)
{
    switch(type.type)
    {
    case TYPE_U1:
    case TYPE_I8:
    case TYPE_U8:
        return 1;
    case TYPE_I16:
    case TYPE_U16:
        return 2;
    case TYPE_I32:
    case TYPE_U32:
        return 4;
    case TYPE_I64:
    case TYPE_U64:
    case TYPE_PTR:
    case TYPE_FUNC:
        return 8;
    default:
        assert(0);
    }
}

void type_cpy(type_t* dst, type_t* src)
{
    memcpy(dst, src, sizeof(type_t));
    switch(dst->type)
    {
    case TYPE_PTR:
        dst->ptrtype = malloc(sizeof(type_t));
        type_cpy(dst->ptrtype, src->ptrtype);
        break;
    case TYPE_FUNC:
        dst->func.ret = malloc(sizeof(type_t));
        type_cpy(dst->func.ret, src->func.ret);
        list_type_dup(&dst->func.args, &src->func.args);
        break;
    case TYPE_STRUCT:
        dst->struc.def = NULL;
        if(src->struc.def)
        {
            dst->struc.def = malloc(sizeof(struct_t));
            struct_cpy(dst->struc.def, src->struc.def);
        }
        break;
    default:
        break;
    }
}

void type_free(type_t* type)
{
    switch(type->type)
    {
    case TYPE_PTR:
        type_free(type->ptrtype);
        free(type->ptrtype);
        break;
    case TYPE_FUNC:
        type_free(type->func.ret);
        free(type->ptrtype);
        list_type_free(&type->func.args);
        break;
    case TYPE_STRUCT:
        // tag is borrowed from the tags map, not owned
        if(type->struc.def)
        {
            struct_free(type->struc.def);
            free(type->struc.def);
        }
    default:
        break;
    }
}

LIST_DEF(type)
LIST_DEF_FREE_DECONSTRUCT(type, type_free)