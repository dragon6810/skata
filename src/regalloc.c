#include "regalloc.h"

#include <assert.h>
#include <stdio.h>

MAP_DEF(uint64_t, ir_regspan_t, u64, ir_regspan, hash_u64, NULL, NULL, NULL, NULL, NULL)

int nreg = 8;

// at this instruction, what hardware register is reg occupying?
// if none/unallocated, -1
int regalloc_hardregatinst(ir_funcdef_t* funcdef, uint64_t iblk, uint64_t iinst, ir_reg_t* reg)
{
    ir_regspan_t *span;

    if(reg->hardreg == -1)
        return -1;

    span = map_u64_ir_regspan_get(&reg->life, &iblk);
    if(!span)
        return -1;

    if(iinst < span->span[0] || iinst >= span->span[1])
        return -1;

    return reg->hardreg;
}

void regalloc_colorreg(ir_funcdef_t* funcdef, ir_reg_t* reg)
{
    int i, j;

    bool openreg[nreg];
    int hardreg;

    for(i=0; i<nreg; i++)
        openreg[i] = true;

    for(i=0; i<reg->life.nbin; i++)
    {
        if(reg->life.bins[i].state != MAP_EL_FULL)
            continue;
        if(!reg->life.bins[i].val.start)
            continue;

        for(j=0; j<funcdef->regs.nbin; j++)
        {
            if(funcdef->regs.bins[j].state != MAP_EL_FULL)
                continue;
            hardreg = regalloc_hardregatinst(funcdef, 
                reg->life.bins[i].key, reg->life.bins[i].val.span[0], 
                &funcdef->regs.bins[j].val);
            if(hardreg >= 0)
                openreg[funcdef->regs.bins[j].val.hardreg] = false;
        }
    }

    for(i=0; i<nreg; i++)
    {
        if(!openreg[i])
            continue;
        reg->hardreg = i;
        return;
    }

    assert(0 && "TODO: spilling");
}

void regalloc_color(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
        if(funcdef->regs.bins[i].state == MAP_EL_FULL)
            regalloc_colorreg(funcdef, &funcdef->regs.bins[i].val);
}

bool regalloc_isread(ir_funcdef_t* funcdef, ir_inst_t* inst, ir_reg_t* reg)
{
    switch(inst->op)
    {
    case IR_OP_RET:
    case IR_OP_BR:
        if(inst->unary.type == IR_OPERAND_REG && map_str_ir_reg_get(&funcdef->regs, &inst->unary.regname) == reg)
            return true;
        break;
    case IR_OP_MOVE:
    case IR_OP_STORE:
        if(inst->binary[1].type == IR_OPERAND_REG && map_str_ir_reg_get(&funcdef->regs, &inst->binary[1].regname) == reg)
            return true;
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
        if(inst->ternary[1].type == IR_OPERAND_REG && map_str_ir_reg_get(&funcdef->regs, &inst->ternary[1].regname) == reg)
            return true;
        if(inst->ternary[2].type == IR_OPERAND_REG && map_str_ir_reg_get(&funcdef->regs, &inst->ternary[2].regname) == reg)
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
        if(!alive && regalloc_isread(funcdef, inst, reg))
        {
            alive = true;
            span.span[1] = i + 1;
            continue;
        }

        // beginning of the register
        if(alive && ir_registerwritten(inst, reg->name))
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
        if(funcdef->regs.bins[i].state == MAP_EL_FULL)
            funcdef->regs.bins[i].val.hardreg = -1;

    regalloc_reglifetime_r(funcdef, reg, funcdef->blocks.len-1, false);
}

void regalloc_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
    {
        if(funcdef->regs.bins[i].state != MAP_EL_FULL)
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

