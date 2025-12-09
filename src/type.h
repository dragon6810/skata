#ifndef _TYPE_H
#define _TYPE_H

#include "map.h"

#define TYPE_FLAG_CONST 0x01

typedef enum
{
    TYPE_VOID=0,
    TYPE_U8,
    TYPE_I8,
    TYPE_U16,
    TYPE_I16,
    TYPE_U32,
    TYPE_I32,
    TYPE_U64,
    TYPE_I64,

    TYPE_COUNT,
} type_e;

extern const char* type_names[TYPE_COUNT];

typedef struct type_s
{
    type_e type;
} type_t;

const char* type_name(type_t* type);

#endif