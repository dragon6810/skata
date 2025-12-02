#include "ir.h"

#include <stdio.h>

list_string_t namestack;

static void ir_rename(ir_funcdef_t* func, ir_var_t* var, ir_block_t* blk)
{
    int i;
    ir_inst_t *inst;

    char *reg;
    uint64_t *pidx, idx;
    ir_operand_t operand;
    uint64_t stacksize;

    stacksize = namestack.len;

    // rename instructions
    for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
    {
        if(inst->op == IR_OP_PHI)
        {
            if(!inst->var || strcmp(inst->var, var->name))
                continue;

            reg = ir_gen_alloctemp(func);
            list_string_push(&namestack, reg);
            inst->variadic.data[0].regname = strdup(reg);
            continue;
        }

        if(inst->op == IR_OP_LOAD)
        {
            if(strcmp(inst->binary[1].var->name, var->name))
                continue;

            inst->op = IR_OP_MOVE;
            inst->binary[1].type = IR_OPERAND_REG;
            inst->binary[1].regname = strdup(namestack.data[namestack.len-1]);
            continue;
        }

        if(inst->op == IR_OP_STORE)
        {
            if(strcmp(inst->binary[0].var->name, var->name))
                continue;

            reg = ir_gen_alloctemp(func);
            list_string_push(&namestack, reg);

            inst->op = IR_OP_MOVE;
            inst->binary[0].type = IR_OPERAND_REG;
            inst->binary[0].regname = strdup(reg);
            continue;
        }
    }

    // rename successor phi-nodes
    for(i=0; i<blk->out.len; i++)
    {
        pidx = map_str_u64_get(&blk->out.data[i]->varphis, var->name);
        if(!pidx)
            continue;
        idx = *pidx;

        inst = &blk->out.data[i]->insts.data[idx];

        operand.type = IR_OPERAND_LABEL;
        operand.label = strdup(blk->name);
        list_ir_operand_ppush(&inst->variadic, &operand);

        operand.type = IR_OPERAND_REG;
        operand.regname = strdup(namestack.data[namestack.len-1]);
        list_ir_operand_ppush(&inst->variadic, &operand);
    }

    // rename children in dominator tree
    for(i=0; i<blk->domchildren.len; i++)
        ir_rename(func, var, blk->domchildren.data[i]);

    while(namestack.len > stacksize)
        list_string_remove(&namestack, namestack.len - 1);
}

// worklist must be initialized
static void ir_populateworklist(list_pir_block_t* worklist, ir_funcdef_t* func, ir_var_t* var)
{
    int i, b;
    ir_block_t *blk;
    ir_inst_t *inst;

    for(b=0, blk=func->blocks.data; b<func->blocks.len; b++, blk++)
    {
        for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
        {
            if(inst->op != IR_OP_STORE && inst->op != IR_OP_PHI)
                continue;
            if(inst->op == IR_OP_STORE && strcmp(inst->binary[0].var->name, var->name))
                continue;
            if(inst->op == IR_OP_PHI && !inst->var)
                continue;
            if(inst->op == IR_OP_PHI && strcmp(inst->var, var->name))
                continue;

            list_pir_block_push(worklist, blk);
            break;
        }
    }
}

void ir_ssafunc(ir_funcdef_t* func)
{
    int v, i;
    
    ir_block_t *blk;
    list_pir_block_t worklist;
    ir_block_t *df;
    ir_inst_t inst;
    uint64_t idx;
    char *reg;

    for(v=0; v<func->vars.nbin; v++)
    {
        if(func->vars.bins[v].state != MAP_EL_FULL)
            continue;
        
        list_pir_block_init(&worklist, 0);
        ir_populateworklist(&worklist, func, &func->vars.bins[v].val);
        while(worklist.len)
        {
            blk = worklist.data[worklist.len - 1];
            list_pir_block_remove(&worklist, worklist.len - 1);

            for(i=0; i<blk->domfrontier.len; i++)
            {
                df = blk->domfrontier.data[i];

                if(map_str_u64_get(&df->varphis, func->vars.bins[v].val.name))
                    continue;

                inst.op = IR_OP_PHI;
                list_ir_operand_init(&inst.variadic, 1);
                inst.variadic.data[0].type = IR_OPERAND_REG;
                inst.variadic.data[0].regname = NULL;
                inst.var = strdup(func->vars.bins[v].val.name);
                
                idx = df->varphis.nfull;
                list_ir_inst_insert(&df->insts, idx, inst);
                map_str_u64_set(&df->varphis, func->vars.bins[v].val.name, idx);

                list_pir_block_push(&worklist, df);
            }
        }
        
        list_pir_block_free(&worklist);
    }

    for(v=0; v<func->vars.nbin; v++)
    {
        if(func->vars.bins[v].state != MAP_EL_FULL)
            continue;

        list_string_init(&namestack, 0);
        for(i=0; i<func->params.len; i++)
        {
            if(strcmp(func->params.data[i].name, func->vars.bins[v].val.name))
                continue;

            reg = ir_gen_alloctemp(func);
            printf("%s: %s\n", func->vars.bins[v].val.name, reg);
            list_string_push(&namestack, reg);
            func->params.data[i].loc.type = IR_LOCATION_REG;
            func->params.data[i].loc.reg = strdup(reg);

            break;
        }
        ir_rename(func, &func->vars.bins[v].val, &func->blocks.data[0]);
        list_string_free(&namestack);

        func->vars.nfull--;
        free(func->vars.bins[v].key);
        ir_varfree(&func->vars.bins[v].val);
        func->vars.bins[v].state = MAP_EL_TOMB;
    }
}

void ir_ssa(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_ssafunc(&ir.defs.data[i]);
}