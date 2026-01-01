#include "ir.h"

#include <assert.h>
#include <stdio.h>

static void ir_print_prim(ir_primitive_e prim)
{
    printf("\e[0;96m");
    switch(prim)
    {
    case IR_PRIM_U1:
        printf("u1");
        break;
    case IR_PRIM_I8:
        printf("i8");
        break;
    case IR_PRIM_U8:
        printf("u8");
        break;
    case IR_PRIM_I16:
        printf("i16");
        break;
    case IR_PRIM_U16:
        printf("u16");
        break;
    case IR_PRIM_I32:
        printf("i32");
        break;
    case IR_PRIM_U32:
        printf("u32");
        break;
    case IR_PRIM_I64:
        printf("i64");
        break;
    case IR_PRIM_U64:
        printf("u64");
        break;
    case IR_PRIM_PTR:
        printf("ptr");
        break;
    default:
        assert(0);
        break;
    }
    printf("\e[0m");
}

void ir_print_type(ir_type_t type)
{
    switch(type.type)
    {
    case IR_TYPE_VOID:
        printf("\e[0;96mvoid\e[0m");
        break;
    case IR_TYPE_PRIM:
        ir_print_prim(type.prim);
        break;
    default:
        assert(0);
        break;
    }
}

void ir_print_location(ir_funcdef_t* funcdef, ir_location_t* location)
{
    ir_reg_t *reg;

    switch(location->type)
    {
    case IR_LOCATION_REG:
        reg = map_str_ir_reg_get(&funcdef->regs, location->reg);
        ir_print_prim(reg->type);
        printf(" \e[0;31m%%%s\e[0m", location->reg);
        break;
    case IR_LOCATION_VAR:
        reg = map_str_ir_reg_get(&funcdef->regs, location->var);
        ir_print_prim(reg->type);
        printf(" \e[0;31m$%s\e[0m", location->var);
        break;
    default:
        assert(0);
        break;
    }
}

void ir_print_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        ir_print_prim(ir_regtype(funcdef, operand->reg.name));
        printf(" \e[0;31m%%%s\e[0m", operand->reg.name);
        break;
    case IR_OPERAND_LIT:
        ir_print_prim(operand->literal.type);
        printf(" \e[0;93m%d\e[0m", operand->literal.i32);
        break;
    case IR_OPERAND_LABEL:
        printf("\e[0;96mlabel \e[0;31m%s\e[0m", operand->label);
        break;
    case IR_OPERAND_FUNC:
        printf("\e[1;93m@%s\e[0m", operand->func);
        break;
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
        ir_print_operand(funcdef, &inst->unary);
        printf("\n");
        break;
    case IR_OP_STORE:
        printf("  \e[0;95mstore\e[0m ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf(" \e[0;95mat\e[0m ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf("\n");
        break;
    case IR_OP_LOAD:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf(" \e[0;95m:= load\e[0m ");
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
    case IR_OP_CMPNEQ:
        printf("  ");
        ir_print_operand(funcdef, &inst->ternary[0]);
        printf(" \e[0;95m:=\e[0m ");
        ir_print_operand(funcdef, &inst->ternary[1]);
        printf(" \e[0;95m!=\e[0m ");
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
        for(i=1; i<inst->variadic.len; i+=2)
        {
            ir_print_operand(funcdef, &inst->variadic.data[i]);
            printf(": ");
            ir_print_operand(funcdef, &inst->variadic.data[i+1]);
            if(i<inst->variadic.len-2)
                printf(", ");
        }
        printf("]");
        if(inst->var)
            printf(" \e[0;36m# $%s", inst->var);
        printf("\n");
        break;
    case IR_OP_CALL:
        printf("  ");
        ir_print_operand(funcdef, &inst->variadic.data[0]);
        printf("\e[0;95m := ");
        ir_print_operand(funcdef, &inst->variadic.data[1]);
        printf("(");
        for(i=2; i<inst->variadic.len; i++)
        {
            ir_print_operand(funcdef, &inst->variadic.data[i]);
            if(i<inst->variadic.len-1)
                printf(", ");
        }
        printf(")\n");
        break;
    case IR_OP_ZEXT:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf("\e[0;95m := zext ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_SEXT:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf("\e[0;95m := sext ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_TRUNC:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf("\e[0;95m := trunc ");
        ir_print_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_ALLOCA:
        printf("  ");
        ir_print_operand(funcdef, &inst->unary);
        printf("\e[0;95m := alloca ");
        ir_print_type(inst->alloca.type);
        printf("\n");
        break;
    default:
        assert(0 && "unkown ir opcode");
        break;
    }
}

void ir_dump_block(ir_funcdef_t* funcdef, ir_block_t* block)
{
    ir_inst_t *inst;

    printf("\e[0;92m%s\e[0m:\n", block->name);

    for(inst=block->insts; inst; inst=inst->next)
        ir_dump_inst(funcdef, inst);
}

void ir_dump_arglist(ir_funcdef_t* funcdef)
{
    int i;

    printf("(");
    for(i=0; i<funcdef->params.len; i++)
    {
        ir_print_location(funcdef, &funcdef->params.data[i].loc);
        if(i != funcdef->params.len-1)
            printf(", ");
    }
    printf(")");
}

void ir_dump_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    ir_print_type(funcdef->rettype);
    printf(" \e[1;93m@%s\e[0m", funcdef->name);
    ir_dump_arglist(funcdef);
    printf(":\n");
    for(i=0; i<funcdef->blocks.len; i++)
        ir_dump_block(funcdef, &funcdef->blocks.data[i]);
    printf("\n");
}

void ir_dump(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_dump_funcdef(&ir.defs.data[i]);
}