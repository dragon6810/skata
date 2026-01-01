#include "back.h"

#include "middle/ir.h"

map_u64_u64_t typereductions;

static ir_primitive_e back_tryreduce(ir_primitive_e type)
{
    uint64_t *newtype;

    newtype = map_u64_u64_get(&typereductions, type);
    if(!newtype)
        return type;
    return *newtype;
}

static void back_reduceblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int o;
    ir_inst_t *inst;
    ir_operand_t *operand;

    list_pir_operand_t operands;

    for(inst=blk->insts; inst; inst=inst->next)
    {
        ir_instoperands(&operands, inst);

        for(o=0; o<operands.len; o++)
        {
            operand = operands.data[o];
            if(operand->type == IR_OPERAND_LIT)
                operand->literal.type = back_tryreduce(operand->literal.type);
        }

        list_pir_operand_free(&operands);
    }
}

static void back_reducefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
        if(funcdef->regs.bins[i].state == MAP_EL_FULL)
            funcdef->regs.bins[i].val.type = back_tryreduce(funcdef->regs.bins[i].val.type);

    for(i=0; i<funcdef->blocks.len; i++)
        back_reduceblk(funcdef, &funcdef->blocks.data[i]);
}

void back_typereduction(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        back_reducefunc(&ir.defs.data[i]);
}

static bool back_instneedsfie(ir_inst_t* inst)
{
    list_pir_operand_t operands;

    ir_instoperands(&operands, inst);
    
    list_pir_operand_free(&operands);

    return false;
}

static void back_fieblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    ir_inst_t *inst;

    for(inst=blk->insts; inst; inst=inst->next)
        back_instneedsfie(inst);
}

static void back_fiefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        back_fieblk(funcdef, &funcdef->blocks.data[i]);
}

void back_fie(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        back_fiefunc(&ir.defs.data[i]);
}