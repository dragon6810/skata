#include "type.h"

const char* type_names[TYPE_COUNT] =
{
    "void",
    "u8",
    "i8",
    "u16",
    "i16",
    "u32",
    "i32",
    "u64",
    "i64",
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