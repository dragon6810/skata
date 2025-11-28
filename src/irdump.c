#include "ir.h"

#include <assert.h>
#include <stdio.h>

bool ir_registerwritten(ir_inst_t* inst, char* reg)
{
    bool iswritten;
    set_str_t set;

    ir_definedregs(&set, inst);
    iswritten = set_str_contains(&set, reg);
    set_str_free(&set);

    return iswritten;
}

void ir_definedregs(set_str_t* set, ir_inst_t* inst)
{
    set_str_alloc(set);

    switch(inst->op)
    {
    case IR_OP_MOVE:
    case IR_OP_LOAD:
        set_str_add(set, inst->binary[0].regname);
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
        set_str_add(set, inst->ternary[0].regname);
        break;
    case IR_OP_PHI:
        if(inst->variadic.len && inst->variadic.data[0].regname)
            set_str_add(set, inst->variadic.data[0].regname);
        break;
    default:
        break;
    }
}

void ir_accessedregs(set_str_t* set, ir_inst_t* inst)
{
    int i;

    set_str_alloc(set);

    switch(inst->op)
    {
    case IR_OP_MOVE:
    case IR_OP_STORE:
        if(inst->binary[1].type == IR_OPERAND_REG) set_str_add(set, inst->binary[1].regname);
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
        if(inst->ternary[1].type == IR_OPERAND_REG) set_str_add(set, inst->ternary[1].regname);
        if(inst->ternary[2].type == IR_OPERAND_REG) set_str_add(set, inst->ternary[2].regname);
        break;
    case IR_OP_RET:
        if(inst->binary[1].type == IR_OPERAND_REG && inst->binary[1].regname) set_str_add(set, inst->binary[1].regname);
        break;
    case IR_OP_BR:
        if(inst->ternary[0].type == IR_OPERAND_REG) set_str_add(set, inst->ternary[0].regname);
        break;
    case IR_OP_PHI:
        for(i=1; i<inst->variadic.len; i++)
            if(inst->variadic.data[i].type == IR_OPERAND_REG) set_str_add(set, inst->variadic.data[i].regname);
        break;
    default:
        break;
    }
}

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
    int i;

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
        ir_print_operand(funcdef, &inst->ternary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[1]);
        printf(" \e[0;95m+\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        break;
    case IR_OP_SUB:
        printf("  ");
        ir_print_operand(funcdef, &inst->ternary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[1]);
        printf(" \e[0;95m-\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        break;
    case IR_OP_MUL:
        printf("  ");
        ir_print_operand(funcdef, &inst->ternary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[1]);
        printf(" \e[0;95m*\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[2]);
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
    case IR_OP_CMPEQ:
        printf("  ");
        ir_print_operand(funcdef, &inst->ternary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[1]);
        printf(" \e[0;95m==\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        break;
    case IR_OP_BR:
        printf("  \e[0;95mbr\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[0]);
        printf(" ? ");
        ir_print_operand(funcdef, &inst->ternary[1]);
        printf(" : ");
        ir_print_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        break;
    case IR_OP_JMP:
        printf("  \e[0;95mbr\e[0m ");
        ir_print_operand(funcdef, &inst->unary);
        printf("\n");
        break;
    case IR_OP_PHI:
        printf("  ");
        ir_print_operand(funcdef, &inst->variadic.data[0]);
        printf("\e[0;95m := phi\e[0m [");
        for(i=1; i<inst->variadic.len; i++)
        {
            ir_print_operand(funcdef, &inst->variadic.data[i]);
            if(i<inst->variadic.len-1)
                printf(", ");
        }
        printf("]");
        if(inst->var)
            printf(" \e[0;36m# $%s", inst->var);
        printf("\n");
        break;
    default:
        assert(0 && "unkown ir opcode");
        break;
    }
}

void ir_dump_vars(ir_funcdef_t* funcdef)
{
    int i;

    ir_var_t *var;

    for(i=0; i<funcdef->vars.nbin; i++)
    {
        if(!funcdef->vars.bins[i].full)
            continue;
        var = &funcdef->vars.bins[i].val;

        printf("  \e[0;95malloca \e[0;31m$%s\e[0m, \e[0;96msize \e[0;93m%d\e[0m\n", var->name, 4);
    }
}

void ir_dump_block(ir_funcdef_t* funcdef, ir_block_t* block)
{
    int i;

    printf("\e[0;92m%s\e[0m:\n", block->name);

    if(!strcmp(block->name, "entry"))
    {
        ir_dump_vars(funcdef);
        printf("\n");
    }

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