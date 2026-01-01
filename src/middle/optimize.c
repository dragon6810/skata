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
    int b, o;
    ir_block_t *blk;
    ir_inst_t *inst;

    char *regname;
    list_pir_operand_t operands;

    assert(!ir_operandhasreg(funcdef, operand, reg));

    regname = strdup(reg);
    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(inst=blk->insts; inst; inst=inst->next)
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
    int b;
    ir_block_t *blk;
    ir_inst_t *inst, *lastinst;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        do
        {
            for(lastinst=NULL, inst=blk->insts; inst; lastinst=inst, inst=inst->next)
            {
                if(inst->op != IR_OP_MOVE)
                    continue;

                assert(inst->binary[0].type == IR_OPERAND_REG);
                
                ir_aliasreg(funcdef, inst->binary[0].reg.name, &inst->binary[1]);
                if(lastinst)
                    lastinst->next = inst->next;
                else
                    blk->insts = inst->next;
                inst->next = NULL;
                ir_instfree(inst);

                madechange = true;

                break;
            }
        } while(inst);
    }
}

static void ir_eliminatelitzext(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    if(inst->binary[1].type != IR_OPERAND_LIT)
        return;

    switch(inst->binary[1].literal.type)
    {
    case IR_PRIM_U1:
    case IR_PRIM_I8:
    case IR_PRIM_U8:
        inst->binary[1].literal.u64 = inst->binary[1].literal.u8;
        break;
    case IR_PRIM_I16:
    case IR_PRIM_U16:
        inst->binary[1].literal.u64 = inst->binary[1].literal.u16;
        break;
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        inst->binary[1].literal.u64 = inst->binary[1].literal.u32;
        break;
    default:
        assert(0);
    }

    inst->op = IR_OP_MOVE;
    inst->binary[1].literal.type = ir_regtype(funcdef, inst->binary[0].reg.name);
    madechange = true;
}

static void ir_eliminatelitsext(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    ir_primitive_e dsttype;

    if(inst->binary[1].type != IR_OPERAND_LIT)
        return;

    dsttype = ir_regtype(funcdef, inst->binary[0].reg.name);
    switch(inst->binary[1].literal.type)
    {
    case IR_PRIM_U1:
    case IR_PRIM_I8:
    case IR_PRIM_U8:
        switch(dsttype)
        {
        case IR_PRIM_I16:
        case IR_PRIM_U16:
            inst->binary[1].literal.i16 = inst->binary[1].literal.i8;
            break;
        case IR_PRIM_I32:
        case IR_PRIM_U32:
            inst->binary[1].literal.i32 = inst->binary[1].literal.i8;
            break;
        case IR_PRIM_I64:
        case IR_PRIM_U64:
            inst->binary[1].literal.i64 = inst->binary[1].literal.i8;
            break;
        default:
            assert(0);
        }
        break;
    case IR_PRIM_I16:
    case IR_PRIM_U16:
        switch(dsttype)
        {
        case IR_PRIM_I32:
        case IR_PRIM_U32:
            inst->binary[1].literal.i32 = inst->binary[1].literal.i16;
            break;
        case IR_PRIM_I64:
        case IR_PRIM_U64:
            inst->binary[1].literal.i64 = inst->binary[1].literal.i16;
            break;
        default:
            assert(0);
        }
        break;
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        switch(dsttype)
        {
        case IR_PRIM_I64:
        case IR_PRIM_U64:
            inst->binary[1].literal.i64 = inst->binary[1].literal.i32;
            break;
        default:
            assert(0);
        }
        break;
    default:
        assert(0);
    }

    inst->op = IR_OP_MOVE;
    inst->binary[1].literal.type = ir_regtype(funcdef, inst->binary[0].reg.name);
    madechange = true;
}

static void ir_eliminatelittrunc(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    if(inst->binary[1].type != IR_OPERAND_LIT)
        return;

    inst->op = IR_OP_MOVE;
    inst->binary[1].literal.type = ir_regtype(funcdef, inst->binary[0].reg.name);
    madechange = true;
}

static void ir_eliminatelitcasts(ir_funcdef_t* funcdef)
{
    int b;
    ir_block_t *blk;
    ir_inst_t *inst;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(inst=blk->insts; inst; inst=inst->next)
        {
            switch(inst->op)
            {
            case IR_OP_ZEXT:
                ir_eliminatelitzext(funcdef, inst);
                break;
            case IR_OP_SEXT:
                ir_eliminatelitsext(funcdef, inst);
                break;
            case IR_OP_TRUNC:
                ir_eliminatelittrunc(funcdef, inst);
                break;
            default:
                break;
            }
        }
    }
}

static void ir_lowersinglephi(ir_funcdef_t* funcdef)
{
    int b;
    ir_block_t *blk;
    ir_inst_t *inst;

    ir_inst_t move;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(inst=blk->insts; inst; inst=inst->next)
        {
            if(inst->op != IR_OP_PHI)
                continue;
            if(inst->variadic.len != 3)
                continue;

            move.op = IR_OP_MOVE;
            ir_cpyoperand(&move.binary[0], &inst->variadic.data[0]);
            ir_cpyoperand(&move.binary[1], &inst->variadic.data[2]);
            move.next = inst->next;

            inst->next = NULL;
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