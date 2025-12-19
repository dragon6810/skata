#include "type.h"

#include <stdio.h>

const char* type_names[TYPE_COUNT] =
{
    "void",
    "i8",
    "u8",
    "i16",
    "u16",
    "i32",
    "u32",
    "i64",
    "u64",
    "func",
};

ir_primitive_e type_toprim(type_e type)
{
    switch(type)
    {
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
    case TYPE_FUNC:
        return IR_PRIM_PTR;
    default:
        printf("attempting to convert non-primitive type %d to an IR primitive!\n", (int) type);
        abort();
    }
}

void type_free(type_t* type)
{
    switch(type->type)
    {
    case TYPE_FUNC:
        free(type->func.ret);
        list_type_free(&type->func.args);
        break;
    default:
        break;
    }
}

LIST_DEF(type)
LIST_DEF_FREE_DECONSTRUCT(type, type_free)