#include "ir.h"

#include <stdio.h>

void ir_regcpy(ir_reg_t* dst, ir_reg_t* src)
{
    memcpy(dst, src, sizeof(ir_reg_t));
    
    dst->name = strdup(src->name);
    set_str_dup(&dst->interfere, &src->interfere);
    dst->hardreg = src->hardreg;
}

void ir_regfree(ir_reg_t* reg)
{
    free(reg->name);
    set_str_free(&reg->interfere);
}

void ir_cpyoperand(ir_operand_t* dst, ir_operand_t* src)
{
    memcpy(dst, src, sizeof(ir_operand_t));
    switch(dst->type)
    {
    case IR_OPERAND_REG:
        if(dst->reg.name)
            dst->reg.name = strdup(dst->reg.name);
        break;
    case IR_OPERAND_LABEL:
        if(dst->label)
            dst->label = strdup(dst->label);
        break;
    case IR_OPERAND_FUNC:
        if(dst->func)
            dst->func = strdup(dst->func);
        break;
    default:
        break;
    }
}

void ir_operandfree(ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        free(operand->reg.name);
        break;
    case IR_OPERAND_LABEL:
        free(operand->label);
        break;
    case IR_OPERAND_FUNC:
        free(operand->func);
        break;
    default:
        break;
    }
}

void ir_instfree(ir_inst_t* inst)
{
    int i;

    int noperand;

    if(!inst)
        return;

    ir_instfree(inst->next);

    if(inst->op == IR_OP_PHI && inst->var)
        free(inst->var);

    switch(inst->op)
    {
    case IR_OP_RET:
    case IR_OP_JMP:
        noperand = 1;
        break;
    case IR_OP_MOVE:
    case IR_OP_STORE:
    case IR_OP_LOAD:
        noperand = 2;
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
    case IR_OP_BR:
        noperand = 3;
        break;
    // variadic
    case IR_OP_PHI:
        list_ir_operand_free(&inst->variadic);
        return;
    default:
        noperand = 0;
        break;
    }

    for(i=0; i<noperand; i++)
        ir_operandfree(&inst->ternary[i]);
    
    free(inst);
}

void ir_freeblock(ir_block_t* block)
{
    free(block->name);
    ir_instfree(block->insts);
    map_str_pir_inst_free(&block->varphis);
    list_pir_block_free(&block->in);
    list_pir_block_free(&block->out);
    set_pir_block_free(&block->dom);
    list_pir_block_free(&block->domfrontier);
    list_pir_block_free(&block->domchildren);
    set_str_free(&block->regdefs);
    set_str_free(&block->reguses);
    set_str_free(&block->livein);
    set_str_free(&block->liveout);
    list_ir_regspan_free(&block->spans);
    list_ir_copy_free(&block->phicpys);
}

void ir_freeparam(ir_param_t* param)
{
    switch(param->loc.type)
    {
    case IR_LOCATION_REG:
        free(param->loc.reg);
        break;
    case IR_LOCATION_VAR:
        free(param->loc.var);
        break;
    default:
        break;
    }
}

void ir_freefuncdef(ir_funcdef_t* funcdef)
{
    free(funcdef->name);
    list_ir_param_free(&funcdef->params);
    map_str_ir_reg_free(&funcdef->regs);
    list_ir_block_free(&funcdef->blocks);
    list_pir_block_free(&funcdef->postorder);
}

static uint64_t ir_hashpblock(ir_block_t** blk)
{
    return (uint64_t) *blk;
}

static uint64_t ir_hashpinst(ir_inst_t** inst)
{
    return (uint64_t) *inst;
}

static void ir_freecopy(ir_copy_t* copy)
{
    ir_operandfree(&copy->src);
}

MAP_DEF(char*, ir_reg_t, str, ir_reg, hash_str, map_strcmp, map_strcpy, ir_regcpy, map_freestr, ir_regfree)
LIST_DEF(pir_inst)
SET_DEF(ir_inst_t*, pir_inst, ir_hashpinst, NULL, NULL, NULL)
LIST_DEF_FREE(pir_inst)
MAP_DEF(char*, ir_inst_t*, str, pir_inst, hash_str, map_strcmp, map_strcpy, NULL, map_freestr, NULL)
LIST_DEF(ir_block)
LIST_DEF_FREE_DECONSTRUCT(ir_block, ir_freeblock)
SET_DEF(ir_block_t*, pir_block, ir_hashpblock, NULL, NULL, NULL)
LIST_DEF(ir_funcdef)
LIST_DEF_FREE_DECONSTRUCT(ir_funcdef, ir_freefuncdef)
LIST_DEF(ir_operand)
LIST_DEF_FREE_DECONSTRUCT(ir_operand, ir_operandfree)
LIST_DEF(pir_operand)
LIST_DEF_FREE(pir_operand)
LIST_DEF(ir_param)
LIST_DEF_FREE_DECONSTRUCT(ir_param, ir_freeparam)
LIST_DEF(ir_copy)
LIST_DEF_FREE_DECONSTRUCT(ir_copy, ir_freecopy)

uint32_t ir_primflags(ir_primitive_e prim)
{
    switch(prim)
    {
    case IR_PRIM_U1:
    case IR_PRIM_U8:
    case IR_PRIM_U16:
    case IR_PRIM_U32:
    case IR_PRIM_U64:
    case IR_PRIM_PTR:
        return IR_FPRIM_INTEGRAL;
    case IR_PRIM_I8:
    case IR_PRIM_I16:
    case IR_PRIM_I32:
    case IR_PRIM_I64:
        return IR_FPRIM_INTEGRAL | IR_FPRIM_SIGNED;
    default:
        assert(0);
    }
}

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
    case IR_OP_ZEXT:
    case IR_OP_SEXT:
    case IR_OP_TRUNC:
    case IR_OP_ALLOCA:
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
    case IR_OP_JMP:
    case IR_OP_ALLOCA:
        break;
    case IR_OP_STORE:
    case IR_OP_LOAD:
        if(inst->binary[0].type == IR_OPERAND_REG) set_str_add(set, inst->binary[0].reg.name);
        if(inst->binary[1].type == IR_OPERAND_REG) set_str_add(set, inst->binary[1].reg.name);
        break;
    case IR_OP_MOVE:
    case IR_OP_ZEXT:
    case IR_OP_SEXT:
    case IR_OP_TRUNC:
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
        printf("unknown opcode %d\n", (int) inst->op);
        assert(0);
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
    case IR_OP_ALLOCA:
        noperands = 1;
        break;
    case IR_OP_MOVE:
    case IR_OP_STORE:
    case IR_OP_LOAD:
    case IR_OP_ZEXT:
    case IR_OP_SEXT:
    case IR_OP_TRUNC:
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

ir_primitive_e ir_regtype(ir_funcdef_t* funcdef, char* regname)
{
    ir_reg_t *reg;

    reg = map_str_ir_reg_get(&funcdef->regs, regname);
    assert(reg);

    return reg->type;
}

int ir_primbytesize(ir_primitive_e prim)
{
    switch(prim)
    {
    case IR_PRIM_I8:
    case IR_PRIM_U8:
        return 1;
    case IR_PRIM_I16:
    case IR_PRIM_U16:
        return 2;
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        return 4;
    case IR_PRIM_PTR:
    case IR_PRIM_I64:
    case IR_PRIM_U64:
        return 8;
    default:
        assert(0);
    }
}