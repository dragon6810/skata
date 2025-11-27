#include "ir.h"

// worklist must be initialized
void ir_populateworklist(list_pir_block_t* worklist, ir_funcdef_t* func, ir_var_t* var)
{
    int i, b;
    ir_block_t *blk;
    ir_inst_t *inst;

    for(b=0, blk=func->blocks.data; b<func->blocks.len; b++, blk++)
    {
        for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
        {
            if(inst->op != IR_OP_STORE)
                continue;
            if(inst->binary[0].var != var)
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

    for(v=0; v<func->vars.nbin; v++)
    {
        if(!func->vars.bins[v].full)
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

                if(map_str_u64_get(&df->varphis, &func->vars.bins[v].val.name))
                    continue;

                inst.op = IR_OP_PHI;
                list_ir_operand_init(&inst.variadic, 1);
                inst.variadic.data[0].type = IR_OPERAND_REG;
                inst.variadic.data[0].regname = NULL;
                inst.var = strdup(func->vars.bins[v].val.name);
                
                idx = df->varphis.nfull;
                list_ir_inst_insert(&df->insts, idx, inst);
                map_str_u64_set(&df->varphis, &func->vars.bins[v].val.name, &idx);

                list_pir_block_push(&worklist, df);
            }
        }
        
        list_pir_block_free(&worklist);
    }
}

void ir_ssa(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_ssafunc(&ir.defs.data[i]);
}