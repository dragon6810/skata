#include "ir.h"

#include <assert.h>
#include <stdio.h>

void ir_print_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("\e[0;31m%%%s\e[0m", operand->regname);
        break;
    case IR_OPERAND_LIT:
        printf("\e[0;93m%d\e[0m", operand->literal.i32);
        break;
    case IR_OPERAND_VAR:
        printf("\e[0;31m$%s\e[0m", operand->var->name);
        break;
    case IR_OPERAND_LABEL:
        printf("\e[0;96mlabel \e[0;31m%s\e[0m", funcdef->blocks.data[operand->ilabel].name);
    default:
        break;
    }
}

void ir_dump_inst(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    switch(inst->op)
    {
    case IR_OP_MOVE:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_ADD:
        printf("  ");
        ir_print_operand(funcdef, &inst->trinary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->trinary[1]);
        printf(" \e[0;95m+\e[0m ");
        ir_print_operand(funcdef, &inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_SUB:
        printf("  ");
        ir_print_operand(funcdef, &inst->trinary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->trinary[1]);
        printf(" \e[0;95m-\e[0m ");
        ir_print_operand(funcdef, &inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_MUL:
        printf("  ");
        ir_print_operand(funcdef, &inst->trinary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->trinary[1]);
        printf(" \e[0;95m*\e[0m ");
        ir_print_operand(funcdef, &inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_RET:
        printf("  ");
        printf("\e[0;95mreturn\e[0m ");
        if(inst->unary.regname)
            ir_print_operand(funcdef, &inst->unary);
        printf("\n");
        break;
    case IR_OP_STORE:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf(" \e[0;95m=\e[0m ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_LOAD:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_BR:
        printf("  \e[0;95mbr\e[0m ");
        ir_print_operand(funcdef, &inst->unary);
        printf("\n");
        break;
    case IR_OP_BZ:
        printf("  \e[0;95mbz\e[0m ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf(", ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    default:
        assert(0 && "unkown ir opcode");
        break;
    }
}

void ir_dump_block(ir_funcdef_t* funcdef, ir_block_t* block)
{
    int i;

    printf("\e[0;92m%s\e[0m:\n", block->name);
    for(i=0; i<block->insts.len; i++)
        ir_dump_inst(funcdef, &block->insts.data[i]);
}

void ir_dump_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    printf("\e[0;96mfunc \e[1;93m@%s\e[0m:\n", funcdef->name);
    for(i=0; i<funcdef->blocks.len; i++)
        ir_dump_block(funcdef, &funcdef->blocks.data[i]);
}

void ir_dump(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_dump_funcdef(&ir.defs.data[i]);
}