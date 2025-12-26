#ifndef _TYPE_H
#define _TYPE_H

#include "middle/ir.h"
#include "map.h"

#define TYPE_FLAG_CONST 0x01

typedef enum
{
    TYPE_VOID=0,

    TYPE_U1, // boolean value, "truth"
    TYPE_I8,
    TYPE_U8,
    TYPE_I16,
    TYPE_U16,
    TYPE_I32,
    TYPE_U32,
    TYPE_I64,
    TYPE_U64,

    TYPE_PTR,
    TYPE_FUNC,

    TYPE_COUNT,
} type_e;

extern const char* type_names[TYPE_COUNT];

typedef struct type_s type_t;

LIST_DECL(type_t, type)

typedef struct type_s
{
    type_e type;

    union
    {
        struct
        {
            type_t *ret;
            list_type_t args;
        } func;
        type_t *ptrtype; // type it points to
    };
} type_t;

ir_primitive_e type_toprim(type_e type);
int type_bytesize(type_t type);

void type_cpy(type_t* dst, type_t* src);
void type_free(type_t* type);

#endif