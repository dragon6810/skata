#include "ir.h"

#include <stdio.h>

LIST_DEF(pir_block)
LIST_DEF_FREE(pir_block)

// a --> b
void ir_flowedge(ir_block_t* a, ir_block_t* b)
{
    list_pir_block_push(&a->out, b);
    list_pir_block_push(&b->in, a);
}

void ir_flowblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;
    ir_inst_t *inst;

    for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
    {
        switch(inst->op)
        {
        case IR_OP_RET:
            ir_flowedge(blk, &funcdef->blocks.data[funcdef->blocks.len-1]);
            return;
        case IR_OP_BR:
            ir_flowedge(blk, &funcdef->blocks.data[inst->ternary[1].ilabel]);
            ir_flowedge(blk, &funcdef->blocks.data[inst->ternary[2].ilabel]);
            return;
        case IR_OP_JMP:
            ir_flowedge(blk, &funcdef->blocks.data[inst->unary.ilabel]);
            return;
        default:
            break;
        }
    }

    if(blk != &funcdef->blocks.data[funcdef->blocks.len-1])
        ir_flowedge(blk, blk+1);
}

void ir_flowfunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        ir_flowblk(funcdef, &funcdef->blocks.data[i]);
}

void ir_flow(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_flowfunc(&ir.defs.data[i]);
}

void ir_dumpflowblk(ir_funcdef_t* func, ir_block_t* blk)
{
    int i;

    for(i=0; i<blk->out.len; i++)
        printf("  %s-->%s\n", blk->name, blk->out.data[i]->name);
}

void ir_dumpflowfunc(ir_funcdef_t* func)
{
    int i;

    printf("graph TD\n\n");

    printf("  title[\"@%s\"]\n", func->name);
    printf("  title-->%s\n", func->blocks.data[0].name);
    printf("  style title fill:#FFF,stroke:#FFF\n");
    printf("  linkStyle 0 stroke:#FFF,stroke-width:0;\n\n");

    for(i=0; i<func->blocks.len; i++)
        ir_dumpflowblk(func, &func->blocks.data[i]);
}

void ir_dumpflow(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_dumpflowfunc(&ir.defs.data[i]);
}