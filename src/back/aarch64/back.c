#include "back/back.h"

#include "middle/ir.h"

static bool back_castreduction_iscastimplicit(ir_primitive_e dst, ir_primitive_e src)
{
    if(dst == IR_PRIM_PTR)
        dst = IR_PRIM_U64;
    if(src == IR_PRIM_PTR)
        src = IR_PRIM_U64;

    if(dst == src)
        return true;

    // signed -> larger
    switch(src)
    {
    case IR_PRIM_I8:
    case IR_PRIM_I16:
    case IR_PRIM_I32:
    case IR_PRIM_I64:
        if(dst > src)
            return false;
    default:
        break;
    }

    return true;
}

static void back_castreduction_block(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;
    ir_inst_t *inst;

    for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
    {
        if(inst->op != IR_OP_CAST)
            continue;
        if(inst->binary[0].type != IR_OPERAND_REG || inst->binary[1].type != IR_OPERAND_REG)
            continue;
        if(!back_castreduction_iscastimplicit(inst->binary[0].reg.type, inst->binary[1].reg.type))
            continue;

        inst->op = IR_OP_MOVE;
    }
}

static void back_castreduction_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        back_castreduction_block(funcdef, &funcdef->blocks.data[i]);
}

void back_castreduction(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        back_castreduction_funcdef(&ir.defs.data[i]);
}