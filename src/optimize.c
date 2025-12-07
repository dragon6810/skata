#include "ir.h"

static void ir_operandreplace(ir_operand_t* dst, ir_operand_t* src)
{
    ir_operand_t newop;

    memcpy(&newop, src, sizeof(ir_operand_t));
    switch(newop.type)
    {
    case IR_OPERAND_REG:
        newop.regname = strdup(newop.regname);
        break;
    case IR_OPERAND_LABEL:
        newop.label = strdup(newop.label);
        break;
    case IR_OPERAND_FUNC:
        newop.label = strdup(newop.func);
    default:
        break;
    }

    ir_operandfree(dst);
    memcpy(dst, &newop, sizeof(ir_operand_t));
}

bool ir_operandeq(ir_funcdef_t* funcdef, ir_operand_t* a, ir_operand_t* b)
{
    ir_reg_t *areg, *breg;

    if(a->type != b->type)
        return false;

    switch(a->type)
    {
    case IR_OPERAND_REG:
        areg = map_str_ir_reg_get(&funcdef->regs, a->regname);
        breg = map_str_ir_reg_get(&funcdef->regs, b->regname);
        if(areg->hardreg && areg->hardreg == breg->hardreg)
            return true;
        if(!strcmp(a->regname, b->regname))
            return true;
        return false;
        return !strcmp(a->regname, b->regname);
    case IR_OPERAND_LIT:
        return a->literal.type == b->literal.type && a->literal.i32 == b->literal.i32;
    case IR_OPERAND_VAR:
        return a->var == b->var;
    case IR_OPERAND_LABEL:
        return !strcmp(a->label, b->label);
    case IR_OPERAND_FUNC:
        return !strcmp(a->func, b->func);
    default:
        assert(0);
        return false;
    }
}

// replace all usages of operand a with operand b, except for assignment during a move
static void ir_replaceoperand(ir_funcdef_t* funcdef, ir_operand_t* opa, ir_operand_t* opb)
{
    int b, i, o;
    ir_block_t *blk;
    ir_inst_t *inst;

    int nops;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
        {
            switch(inst->op)
            {
            case IR_OP_RET:
            case IR_OP_JMP:
                nops = 1;
                break;
            case IR_OP_MOVE:
            case IR_OP_STORE:
            case IR_OP_LOAD:
                nops = 2;
                break;
            case IR_OP_ADD:
            case IR_OP_SUB:
            case IR_OP_MUL:
            case IR_OP_CMPEQ:
            case IR_OP_BR:
                nops = 3;
                break;
            case IR_OP_PHI:
            case IR_OP_CALL:
                nops = 0;
                for(o=0; o<inst->variadic.len; o++)
                    if(ir_operandeq(funcdef, &inst->variadic.data[o], opa))
                        ir_operandreplace(&inst->variadic.data[o], opb);
                break;
            default:
                break;
            }

            o = 0;
            if(inst->op == IR_OP_MOVE)
                o = 1;
            for(; o<nops; o++)
                if(ir_operandeq(funcdef, &inst->ternary[o], opa))
                    ir_operandreplace(&inst->ternary[o], opb);
        }
    }
}

void ir_eliminatemoves(ir_funcdef_t* funcdef)
{
    int b, i;
    ir_block_t *blk;
    ir_inst_t *inst;

    bool changed;

    for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
    {
        do
        {
            changed = false;
            for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
            {
                if(inst->op != IR_OP_MOVE)
                    continue;
                if(inst->binary[0].type != IR_OPERAND_REG)
                    continue;

                ir_replaceoperand(funcdef, &inst->binary[0], &inst->binary[1]);
                ir_operandfree(&inst->binary[0]);
                list_ir_inst_remove(&blk->insts, i);
                changed = true;
                break;
            }
        } while(changed);
    }
}

void ir_lowersinglephi(ir_funcdef_t* funcdef)
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
        }
    }
}

void ir_middleoptimize(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
    {
        ir_lowersinglephi(&ir.defs.data[i]);
        ir_eliminatemoves(&ir.defs.data[i]);
    }
}

void ir_backoptimize(void)
{
    
}