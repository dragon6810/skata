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
    case IR_OP_RET:
    case IR_OP_STORE:
    case IR_OP_BR:
    case IR_OP_JMP:
        break;
    case IR_OP_MOVE:
    case IR_OP_LOAD:
    case IR_OP_CAST:
        set_str_add(set, inst->binary[0].reg.name);
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
    case IR_OP_CMPNEQ:
        set_str_add(set, inst->ternary[0].reg.name);
        break;
    case IR_OP_PHI:
    case IR_OP_CALL:
        if(inst->variadic.len && inst->variadic.data[0].reg.name)
            set_str_add(set, inst->variadic.data[0].reg.name);
        break;
    default:
        assert(0);
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
    case IR_OP_CAST:
        if(inst->binary[1].type == IR_OPERAND_REG) set_str_add(set, inst->binary[1].reg.name);
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
    case IR_OP_CMPNEQ:
        if(inst->ternary[1].type == IR_OPERAND_REG) set_str_add(set, inst->ternary[1].reg.name);
        if(inst->ternary[2].type == IR_OPERAND_REG) set_str_add(set, inst->ternary[2].reg.name);
        break;
    case IR_OP_RET:
        if(inst->unary.type == IR_OPERAND_REG && inst->unary.reg.name) set_str_add(set, inst->unary.reg.name);
        break;
    case IR_OP_BR:
        if(inst->ternary[0].type == IR_OPERAND_REG) set_str_add(set, inst->ternary[0].reg.name);
        break;
    case IR_OP_PHI:
        // skip over labels
        for(i=2; i<inst->variadic.len; i+=2)
            if(inst->variadic.data[i].type == IR_OPERAND_REG) set_str_add(set, inst->variadic.data[i].reg.name);
        break;
    case IR_OP_CALL:
        for(i=2; i<inst->variadic.len; i++)
            if(inst->variadic.data[i].type == IR_OPERAND_REG) set_str_add(set, inst->variadic.data[i].reg.name);
        break;
    default:
        break;
    }
}

void ir_instoperands(list_pir_operand_t* list, ir_inst_t* inst)
{
    int i;

    int noperands;
    
    assert(inst);

    switch(inst->op)
    {
    case IR_OP_RET:
        noperands = inst->hasval;
        break;
    case IR_OP_JMP:
        noperands = 1;
        break;
    case IR_OP_MOVE:
    case IR_OP_STORE:
    case IR_OP_LOAD:
    case IR_OP_CAST:
        noperands = 2;
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
    case IR_OP_CMPNEQ:
    case IR_OP_BR:
        noperands = 3;
        break;
    case IR_OP_PHI:
    case IR_OP_CALL:
        list_pir_operand_init(list, inst->variadic.len);
        for(i=0; i<inst->variadic.len; i++)
            list->data[i] = &inst->variadic.data[i];
        return;
    default:
        assert(0);
    }

    list_pir_operand_init(list, noperands);
    for(i=0; i<noperands; i++)
        list->data[i] = &inst->ternary[i];
}

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
    ir_var_t *var;

    switch(location->type)
    {
    case IR_LOCATION_REG:
        reg = map_str_ir_reg_get(&funcdef->regs, location->reg);
        ir_print_prim(reg->type);
        printf(" \e[0;31m%%%s\e[0m", location->reg);
        break;
    case IR_LOCATION_VAR:
        var = map_str_ir_var_get(&funcdef->vars, location->var);
        ir_print_type(var->type);
        printf(" \e[0;31m$%s\e[0m", location->var);
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
        ir_print_prim(ir_regtype(funcdef, operand->reg.name));
        printf(" \e[0;31m%%%s\e[0m", operand->reg.name);
        break;
    case IR_OPERAND_LIT:
        ir_print_prim(operand->literal.type);
        printf(" \e[0;93m%d\e[0m", operand->literal.i32);
        break;
    case IR_OPERAND_VAR:
        printf("\e[0;31m$%s\e[0m", operand->var->name);
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
    case IR_OP_CAST:
        printf("  ");
        ir_print_operand(funcdef, &inst->binary[0]);
        printf("\e[0;95m := cast ");
        ir_print_operand(funcdef, &inst->binary[1]);
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
        if(funcdef->vars.bins[i].state != MAP_EL_FULL)
            continue;
        var = &funcdef->vars.bins[i].val;

        printf("  \e[0;95malloca \e[0;31m$%s\e[0m, \e[0;96msize \e[0;93m%d\e[0m\n", var->name, 4);
    }
}

void ir_dump_block(ir_funcdef_t* funcdef, ir_block_t* block)
{
    int i;

    printf("\e[0;92m%s\e[0m:\n", block->name);

    if(!strcmp(block->name, "entry") && funcdef->vars.nfull)
    {
        ir_dump_vars(funcdef);
        printf("\n");
    }

    for(i=0; i<block->insts.len; i++)
        ir_dump_inst(funcdef, &block->insts.data[i]);
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