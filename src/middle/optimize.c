#include "ir.h"

#include <stdio.h>

static bool madechange;

// preserves dst type
static void ir_operandreplace(ir_operand_t* dst, ir_operand_t* src)
{
    ir_operand_t newop;

    if(dst == src)
        return;

    ir_cpyoperand(&newop, src);
    ir_operandfree(dst);

    memcpy(dst, &newop, sizeof(ir_operand_t));
}

static bool ir_operandhasreg(ir_funcdef_t* funcdef, ir_operand_t* operand, const char* reg)
{
    if(operand->type != IR_OPERAND_REG)
        return false;
    if(strcmp(operand->reg.name, reg))
        return false;
    return true;
}

// replace all usages of reg with operand
static void ir_aliasreg(ir_funcdef_t* funcdef, const char* reg, ir_operand_t* operand)
{
    int b, i, o;
    ir_block_t *blk;
    ir_inst_t *inst;

    char *regname;
    list_pir_operand_t operands;

    assert(!ir_operandhasreg(funcdef, operand, reg));

    regname = strdup(reg);
    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
        {
            ir_instoperands(&operands, inst);

            for(o=0; o<operands.len; o++)
                if(ir_operandhasreg(funcdef, operands.data[o], regname))
                    ir_operandreplace(operands.data[o], operand);

            list_pir_operand_free(&operands);
        }
    }
    free(regname);
}

static void ir_eliminatemoves(ir_funcdef_t* funcdef)
{
    int b, i;
    ir_block_t *blk;
    ir_inst_t *inst;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        do
        {
            for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
            {
                if(inst->op != IR_OP_MOVE)
                    continue;

                assert(inst->binary[0].type == IR_OPERAND_REG);
                
                ir_aliasreg(funcdef, inst->binary[0].reg.name, &inst->binary[1]);
                list_ir_inst_remove(&blk->insts, i);

                madechange = true;

                break;
            }
        } while(i < blk->insts.len);
    }
}

static void ir_convertlit(ir_constant_t* lit, ir_primitive_e type)
{
    // TODO: conversions for some types

    lit->type = type;
}

static void ir_eliminatelitcasts(ir_funcdef_t* funcdef)
{
    int b, i;
    ir_block_t *blk;
    ir_inst_t *inst;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
        {
            if(inst->op != IR_OP_CAST)
                continue;
            if(inst->binary[1].type != IR_OPERAND_LIT)
                continue;

            inst->op = IR_OP_MOVE;
            ir_convertlit(&inst->binary[1].literal, ir_regtype(funcdef, inst->binary[0].reg.name));

            madechange = true;
        }
    }
}

static void ir_lowersinglephi(ir_funcdef_t* funcdef)
{
    int b, i;
    ir_block_t *blk;
    ir_inst_t *inst;

    ir_inst_t move;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
        {
            if(inst->op != IR_OP_PHI)
                continue;
            if(inst->variadic.len != 3)
                continue;

            move.op = IR_OP_MOVE;
            ir_cpyoperand(&move.binary[0], &inst->variadic.data[0]);
            ir_cpyoperand(&move.binary[1], &inst->variadic.data[2]);

            ir_instfree(inst);
            *inst = move;

            madechange = true;
        }
    }
}

void optimize(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
    {
        do
        {
            madechange = false;

            ir_lowersinglephi(&ir.defs.data[i]);
            ir_eliminatelitcasts(&ir.defs.data[i]);
            ir_eliminatemoves(&ir.defs.data[i]);
        } while(madechange);
    }
}