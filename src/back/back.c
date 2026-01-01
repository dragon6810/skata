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

static void back_instfie(ir_funcdef_t* func, ir_block_t* blk, ir_inst_t* last, ir_inst_t* inst)
{
    int i;

    list_pir_operand_t operands;
    ir_reg_t *reg;
    ir_inst_t *fie;

    ir_instoperands(&operands, inst);

    if(inst->op == IR_OP_STORE)
        list_pir_operand_remove(&operands, 0);
    if(inst->op == IR_OP_LOAD)
        list_pir_operand_remove(&operands, 1);

    for(i=0; i<operands.len; i++)
    {
        if(operands.data[i]->type != IR_OPERAND_REG)
            continue;
        reg = map_str_ir_reg_get(&func->regs, operands.data[i]->reg.name);
        assert(reg);
        if(!reg->def || reg->def->op != IR_OP_ALLOCA)
            continue;
        
        fie = malloc(sizeof(ir_inst_t));
        fie->op = IR_OP_FIE;
        fie->binary[0].type = IR_OPERAND_REG;
        fie->binary[0].reg.name = ir_gen_alloctemp(func, IR_PRIM_PTR);
        fie->binary[1].type = IR_OPERAND_REG;
        fie->binary[1].reg.name = operands.data[i]->reg.name;
        fie->next = inst;
        if(last)
            last->next = fie;
        else
            blk->insts = fie;
        last = fie;

        operands.data[i]->reg.name = strdup(fie->binary[0].reg.name);
    }
    
    list_pir_operand_free(&operands);
}

static void back_fieblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    ir_inst_t *inst, *last;

    for(last=NULL, inst=blk->insts; inst; last=inst, inst=inst->next)
        back_instfie(funcdef, blk, last, inst);
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