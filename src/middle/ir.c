#include "ir.h"

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
    case IR_PRIM_I64:
    case IR_PRIM_U64:
        return 8;
    default:
        assert(0);
    }
}