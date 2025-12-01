#include "ir.h"

#include <stdio.h>

static void ir_findphicpys(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    int i;
    ir_copy_t cpy;

    uint64_t *ilabel;
    ir_block_t *blk;

    assert(inst->op == IR_OP_PHI);

    for(i=2; i<inst->variadic.len; i+=2)
    {
        ir_cpyoperand(&cpy.dst, &inst->variadic.data[0]);
        ir_cpyoperand(&cpy.src, &inst->variadic.data[i]);

        ilabel = map_str_u64_get(&funcdef->blktbl, inst->variadic.data[i-1].label);
        assert(ilabel);
        blk = &funcdef->blocks.data[*ilabel];

        list_ir_copy_ppush(&blk->phicpys, &cpy);
    }
}

static void ir_findblkcpys(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;

    for(i=0; i<blk->insts.len; i++)
    {
        if(blk->insts.data[i].op != IR_OP_PHI)
            continue;
            
        ir_findphicpys(funcdef, &blk->insts.data[i]);
        list_ir_inst_remove(&blk->insts, i);
        i--;
    }
}

static void ir_elimcriticaledges(ir_funcdef_t* funcdef)
{
    int b, e;
    ir_block_t *blk;

    ir_block_t *edge;
    ir_inst_t inst;
    uint64_t idx;
    bool foundedge;

    do
    {
        foundedge = false;
        for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
        {
            if(blk->out.len <= 1)
                continue;
            
            for(e=0; e<blk->out.len; e++)
            {
                edge = blk->out.data[e];
                if(edge->in.len <= 1)
                    continue;

                // critical edge!

                inst.op = IR_OP_JMP;
                inst.unary.type = IR_OPERAND_LABEL;
                inst.unary.label = strdup(edge->name);

                idx = ir_newblock(funcdef);
                blk = &funcdef->blocks.data[b];
                if(!strcmp(blk->insts.data[blk->insts.len-1].ternary[1].label, edge->name))
                {
                    free(blk->insts.data[blk->insts.len-1].ternary[1].label);
                    blk->insts.data[blk->insts.len-1].ternary[1].label 
                        = strdup(funcdef->blocks.data[idx].name);
                }
                if(!strcmp(blk->insts.data[blk->insts.len-1].ternary[2].label, edge->name))
                {
                    free(blk->insts.data[blk->insts.len-1].ternary[2].label);
                    blk->insts.data[blk->insts.len-1].ternary[2].label
                        = strdup(funcdef->blocks.data[idx].name);
                }
                list_ir_inst_ppush(&funcdef->blocks.data[idx].insts, &inst);
                ir_flow();

                foundedge = true;
                break;
            }

            if(foundedge)
                break;
        }
    } while(foundedge);
}

static void ir_lowerfunc(ir_funcdef_t* funcdef)
{
    int i;

    ir_elimcriticaledges(funcdef);

    for(i=0; i<funcdef->blocks.len; i++)
        ir_findblkcpys(funcdef, &funcdef->blocks.data[i]);
}

void ir_lower(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_lowerfunc(&ir.defs.data[i]);
}