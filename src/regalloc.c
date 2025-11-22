#include "regalloc.h"

#include <assert.h>
#include <stdio.h>

MAP_DEF(uint64_t, ir_regspan_t, u64, ir_regspan, NULL, NULL, NULL, NULL, NULL, NULL)

int nreg = 31;

void regalloc_colorblock(ir_funcdef_t* funcdef, ir_block_t* block, bool* hardocc)
{

}

void regalloc_color(ir_funcdef_t* funcdef)
{
    int i;

    bool hardocc[nreg];

    for(i=0; i<nreg; i++)
        hardocc[i] = false;

    for(i=0; i<funcdef->blocks.len; i++)
        regalloc_colorblock(funcdef, &funcdef->blocks.data[i], hardocc);
}

bool regalloc_iswritten(ir_inst_t* inst, ir_reg_t* reg)
{
    switch(inst->op)
    {
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_MOVE:
    case IR_OP_LOAD:
        if(inst->unary.type == IR_OPERAND_REG && inst->unary.reg == reg)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool regalloc_isread(ir_inst_t* inst, ir_reg_t* reg)
{
    switch(inst->op)
    {
    case IR_OP_BZ:
        if(inst->unary.type == IR_OPERAND_REG && inst->unary.reg == reg)
            return true;
        break;
    case IR_OP_MOVE:
    case IR_OP_RET:
    case IR_OP_STORE:
        if(inst->binary[1].type == IR_OPERAND_REG && inst->binary[1].reg == reg)
            return true;
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
        if(inst->trinary[1].type == IR_OPERAND_REG && inst->trinary[1].reg == reg)
            return true;
        if(inst->trinary[2].type == IR_OPERAND_REG && inst->trinary[2].reg == reg)
            return true;
        break;
    default:
        break;
    }

    return false;
}

void regalloc_reglifetime_r(ir_funcdef_t* funcdef, ir_reg_t* reg, uint64_t iblk, bool alive)
{
    int i;

    ir_block_t *blk;

    ir_inst_t *inst;
    ir_regspan_t span;

    blk = &funcdef->blocks.data[iblk];
    if(blk->marked)
        return;
    blk->marked = true;

    span.start = false;
    span.span[0] = 0;
    span.span[1] = alive ? blk->insts.len : 0;

    for(i=blk->insts.len-1; i>=0; i--)
    {
        inst = &blk->insts.data[i];

        // last time the register is ever used
        if(!alive && regalloc_isread(inst, reg))
        {
            alive = true;
            span.span[1] = i + 1;
            continue;
        }

        // beginning of the register
        if(alive && regalloc_iswritten(inst, reg))
        {
            span.start = true;
            span.span[0] = i;
            break;
        }
    }

    if(alive)
        map_u64_ir_regspan_set(&reg->life, &iblk, &span);

    // we found the first instruction, so we dont need to go back further
    if(span.start)
        return;

    for(i=0; i<blk->in.len; i++)
        regalloc_reglifetime_r(funcdef, reg, blk->in.data[i] - funcdef->blocks.data, alive);
}

void regalloc_reglifetime(ir_funcdef_t* funcdef, ir_reg_t* reg)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        funcdef->blocks.data[i].marked = false;

    for(i=0; i<funcdef->regs.nbin; i++)
        if(funcdef->regs.bins[i].full)
            funcdef->regs.bins[i].val.hardreg = -1;

    regalloc_reglifetime_r(funcdef, reg, funcdef->blocks.len-1, false);
}

void regalloc_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
    {
        if(!funcdef->regs.bins[i].full)
            continue;
        regalloc_reglifetime(funcdef, &funcdef->regs.bins[i].val);
    }

    regalloc_color(funcdef);
}

void regalloc(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        regalloc_funcdef(&ir.defs.data[i]);
}

