#include "ir.h"

#include <stdio.h>

static void ir_lowerphi(ir_funcdef_t* funcdef, char* dst, char* src)
{
    int i, b;
    ir_block_t *blk;
    
    ir_inst_t inst;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(i=0; i<blk->insts.len; i++)
        {
            if(!ir_registerwritten(&blk->insts.data[i], src))
                continue;
            
            inst.op = IR_OP_MOVE;
            inst.binary[0].type = IR_OPERAND_REG;
            inst.binary[0].regname = strdup(dst);
            inst.binary[1].type = IR_OPERAND_REG;
            inst.binary[1].regname = strdup(src);
            list_ir_inst_insert(&blk->insts, i+1, inst);
            i++;
        }
    }
}

static void ir_lowerblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i, j;

    for(i=0; i<blk->insts.len; i++)
    {
        switch(blk->insts.data[i].op)
        {
        case IR_OP_PHI:
            for(j=0; j<blk->insts.data[i].variadic.len; j++)
                ir_lowerphi(funcdef, blk->insts.data[i].variadic.data[0].regname, blk->insts.data[i].variadic.data[j].regname);
            list_ir_inst_remove(&blk->insts, i);
            i--;
            break;
        default:
            break;
        }
    }
}

static void ir_lowerfunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        ir_lowerblk(funcdef, &funcdef->blocks.data[i]);
}

void ir_lower(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_lowerfunc(&ir.defs.data[i]);
}