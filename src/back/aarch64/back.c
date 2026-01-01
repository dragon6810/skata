#include "back/back.h"

#include "middle/ir.h"

bool back_canindirect(ir_inst_e opcode, int ioperand)
{
    if(opcode == IR_OP_STORE && ioperand == 0)
        return true;
    if(opcode == IR_OP_LOAD && ioperand == 1)
        return true;
    return false;
}