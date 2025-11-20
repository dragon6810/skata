#include "regalloc.h"

#include <assert.h>
#include <stdio.h>

int nreg = 31;

int regalloc_getnoperands(ir_inst_e opcode)
{
    switch (opcode)
    {
    case IR_OP_MOVE:
    case IR_OP_STORE:
    case IR_OP_LOAD:
        return 2;
    case IR_OP_RET:
        return 1;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
        return 3;
    default:
        assert(0);
    }
}

void regalloc_colorblock(ir_funcdef_t* funcdef, ir_block_t* block, bool* hardocc)
{
    int i, j, k;

    int noperand;
    ir_reg_t *reg;

    for(i=0; i<block->insts.len; i++)
    {
        noperand = regalloc_getnoperands(block->insts.data[i].op);

        for(j=0; j<noperand; j++)
        {
            if(block->insts.data[i].trinary[j].type != IR_OPERAND_REG)
                continue;

            reg = block->insts.data[i].trinary[j].reg;

            if(reg->life[0] != i)
                continue;

            for(k=0; k<nreg; k++)
            {
                if(hardocc[k])
                    continue;
                hardocc[k] = true;
                reg->hardreg = k;
                break;
            }

            assert(k < nreg && "TODO: spilling\n");
        }

        for(j=0; j<noperand; j++)
        {
            if(block->insts.data[i].trinary[j].type != IR_OPERAND_REG)
                continue;

            reg = block->insts.data[i].trinary[j].reg;
            
            if(reg->life[1] != i)
                continue;

            hardocc[reg->hardreg] = false;
        }
    }
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

void regalloc_reglifetime(ir_funcdef_t* funcdef, ir_reg_t* reg)
{
    /*
    int b, i, j;

    int noperand;

    reg->life[0] = reg->life[1] = -1;
    for(i=0; i<funcdef->insts.len; i++)
    {
        noperand = regalloc_getnoperands(funcdef->insts.data[i].op);
        for(j=0; j<noperand; j++)
            if((funcdef->insts.data[i].trinary[j].type == IR_OPERAND_REG) && (funcdef->insts.data[i].trinary[j].reg == reg))
                break;
        if(j >= noperand)
            continue;

        // reg was referenced this instruction

        if(reg->life[0] == -1)
            reg->life[0] = i;
        reg->life[1] = i;
    }
    */
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

