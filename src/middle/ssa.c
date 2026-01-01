#include "ir.h"

#include <stdio.h>

list_string_t namestack;

static void ir_deleterenamed(ir_funcdef_t* func, ir_block_t* blk, set_pir_inst_t* renamed)
{
    ir_inst_t *inst, *last, *next;

    for(last=NULL, inst=blk->insts; inst; last=inst, inst=next)
    {
        next = inst->next;

        if(!set_pir_inst_contains(renamed, inst))
            continue;

        assert(inst->op == IR_OP_ALLOCA);
        assert(inst->unary.type == IR_OPERAND_REG);

        map_str_ir_reg_remove(&func->regs, inst->unary.reg.name);

        if(last)
            last->next = inst->next;
        else
            blk->insts = inst->next;
        
        inst->next = NULL;
        ir_instfree(inst);
    }
}

static void ir_rename(ir_funcdef_t* func, char* varreg, ir_primitive_e type, ir_block_t* blk)
{
    int i;

    char *reg;
    ir_inst_t **pinst, *inst;
    ir_operand_t operand;
    uint64_t stacksize;

    stacksize = namestack.len;

    // rename instructions
    for(inst=blk->insts; inst; inst=inst->next)
    {
        if(inst->op == IR_OP_PHI)
        {
            if(!inst->var || strcmp(inst->var, varreg))
                continue;

            reg = ir_gen_alloctemp(func, type);
            list_string_push(&namestack, reg);
            inst->variadic.data[0].reg.name = strdup(reg);
            continue;
        }

        if(inst->op == IR_OP_LOAD)
        {
            if(strcmp(inst->binary[1].reg.name, varreg))
                continue;

            inst->op = IR_OP_MOVE;
            inst->binary[1].type = IR_OPERAND_REG;
            inst->binary[1].reg.name = strdup(namestack.data[namestack.len-1]);
            continue;
        }

        if(inst->op == IR_OP_STORE)
        {
            if(strcmp(inst->binary[0].reg.name, varreg))
                continue;

            reg = ir_gen_alloctemp(func, type);
            list_string_push(&namestack, reg);

            inst->op = IR_OP_MOVE;
            inst->binary[0].type = IR_OPERAND_REG;
            inst->binary[0].reg.name = strdup(reg);
            continue;
        }
    }

    if(namestack.len)
    {
        // rename successor phi-nodes
        for(i=0; i<blk->out.len; i++)
        {
            pinst = map_str_pir_inst_get(&blk->out.data[i]->varphis, varreg);
            if(!pinst)
                continue;
            inst = *pinst;

            operand.type = IR_OPERAND_LABEL;
            operand.label = strdup(blk->name);
            list_ir_operand_ppush(&inst->variadic, &operand);

            operand.type = IR_OPERAND_REG;
            operand.reg.name = strdup(namestack.data[namestack.len-1]);
            list_ir_operand_ppush(&inst->variadic, &operand);
        }
    }

    // rename children in dominator tree
    for(i=0; i<blk->domchildren.len; i++)
        ir_rename(func, varreg, type, blk->domchildren.data[i]);

    while(namestack.len > stacksize)
        list_string_remove(&namestack, namestack.len - 1);
}

// worklist must be initialized
static void ir_populateworklist(list_pir_block_t* worklist, ir_funcdef_t* func, char* varreg, ir_primitive_e type)
{
    int b;
    ir_block_t *blk;
    ir_inst_t *inst;

    for(b=0, blk=func->blocks.data; b<func->blocks.len; b++, blk++)
    {
        for(inst=blk->insts; inst; inst=inst->next)
        {
            if(inst->op != IR_OP_STORE && inst->op != IR_OP_PHI)
                continue;
            if(inst->op == IR_OP_STORE && strcmp(inst->binary[0].reg.name, varreg))
                continue;
            if(inst->op == IR_OP_PHI && !inst->var)
                continue;
            if(inst->op == IR_OP_PHI && strcmp(inst->var, varreg))
                continue;

            list_pir_block_push(worklist, blk);
            break;
        }
    }
}

static bool ir_varssaable(ir_funcdef_t* func, const char* var)
{
    int b, o;
    ir_block_t *blk;
    ir_inst_t *inst;

    list_pir_operand_t operands;

    for(b=0, blk=func->blocks.data; b<func->blocks.len; b++, blk++)
    {
        for(inst=blk->insts; inst; inst=inst->next)
        {
            if(inst->op == IR_OP_ALLOCA)
                continue;
            
            ir_instoperands(&operands, inst);
            if(inst->op == IR_OP_STORE)
                list_pir_operand_remove(&operands, 0);
            if(inst->op == IR_OP_LOAD)
                list_pir_operand_remove(&operands, 1);

            for(o=0; o<operands.len; o++)
            {
                if(operands.data[o]->type != IR_OPERAND_REG)
                    continue;
                if(strcmp(operands.data[o]->reg.name, var))
                    continue;
                
                list_pir_operand_free(&operands);
                return false;
            }

            list_pir_operand_free(&operands);
        }
    }

    return true;
}

// insts shouldnt be intialized
// sorted from first to last
static void ir_populatealloclist(list_pir_inst_t* insts, ir_funcdef_t* func, ir_block_t* blk)
{
    ir_inst_t *inst;

    list_pir_inst_init(insts, 0);

    for(inst=blk->insts; inst; inst=inst->next)
    {
        if(inst->op != IR_OP_ALLOCA)
            continue;
        if(!ir_varssaable(func, inst->unary.reg.name))
            continue;

        list_pir_inst_push(insts, inst);
    }
}

static void ir_ssafunc(ir_funcdef_t* func)
{
    int v, i;
    
    ir_block_t *entry, *blk;
    list_pir_inst_t insts;
    list_pir_block_t worklist;
    ir_block_t *df;
    ir_inst_t *inst, *pinst;
    set_pir_inst_t renamed;

    set_pir_inst_alloc(&renamed);

    entry = func->blocks.data;
    ir_populatealloclist(&insts, func, entry);

    for(v=0; v<insts.len; v++)
    {
        pinst = insts.data[v];

        list_pir_block_init(&worklist, 0);
        ir_populateworklist(&worklist, func, pinst->unary.reg.name, pinst->alloca.type.prim);
        while(worklist.len)
        {
            blk = worklist.data[worklist.len - 1];
            list_pir_block_remove(&worklist, worklist.len - 1);

            for(i=0; i<blk->domfrontier.len; i++)
            {
                df = blk->domfrontier.data[i];

                if(map_str_pir_inst_get(&df->varphis, pinst->unary.reg.name))
                    continue;

                inst = malloc(sizeof(ir_inst_t));
                inst->op = IR_OP_PHI;
                list_ir_operand_init(&inst->variadic, 1);
                inst->variadic.data[0].type = IR_OPERAND_REG;
                inst->variadic.data[0].reg.name = NULL;
                inst->var = strdup(pinst->unary.reg.name);
                
                inst->next = df->insts;
                df->insts = inst;
                map_str_pir_inst_set(&df->varphis, pinst->unary.reg.name, inst);

                list_pir_block_push(&worklist, df);
            }
        }
        
        list_pir_block_free(&worklist);
    }

    for(v=0; v<insts.len; v++)
    {
        pinst = insts.data[v];
        list_string_init(&namestack, 0);
        ir_rename(func, pinst->unary.reg.name, pinst->alloca.type.prim, entry);
        list_string_free(&namestack);
        set_pir_inst_add(&renamed, pinst);
    }

    ir_deleterenamed(func, entry, &renamed);

    set_pir_inst_free(&renamed);
    list_pir_inst_free(&insts);
}

void ssa(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_ssafunc(&ir.defs.data[i]);
}