#include "type.h"

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