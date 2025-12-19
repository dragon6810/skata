#include "ir.h"

int ir_primbytesize(ir_primitive_e prim)
{
    switch(prim)
    {
    case IR_PRIM_I8:
    case IR_PRIM_U8:
        return 1;
    case IR_PRIM_I16:
    case IR_PRIM_U16:
        return 2;
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        return 4;
    case IR_PRIM_I64:
    case IR_PRIM_U64:
        return 8;
    default:
        assert(0);
    }
}