#include "ir.h"

#include <stdio.h>

LIST_DEF(pir_block)
LIST_DEF_FREE(pir_block)

ir_block_t* ir_idom(ir_block_t* blk)
{
    int i, j;

    for(i=0; i<blk->dom.len; i++)
    {
        if(blk->dom.data[i] == blk)
            continue;

        for(j=0; j<blk->dom.len; j++)
        {
            if(i == j || blk->dom.data[j] == blk)
                continue;
            if(list_pir_block_find(&blk->dom.data[j]->dom, blk->dom.data[i]))
                break;
        }

        if(j < blk->dom.len)
            continue;

        return blk->dom.data[i];
    }

    return NULL;
}

void ir_domtreefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
    {
        funcdef->blocks.data[i].idom = ir_idom(&funcdef->blocks.data[i]);
        if(funcdef->blocks.data[i].idom)
            list_pir_block_push(&funcdef->blocks.data[i].idom->domchildren, &funcdef->blocks.data[i]);
    }
}

void ir_domfrontierblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i, j;

    for(i=0; i<funcdef->blocks.len; i++)
    {
        if(&funcdef->blocks.data[i] == blk)
            continue;
        if(list_pir_block_find(&funcdef->blocks.data[i].dom, blk))
            continue;

        for(j=0; j<funcdef->blocks.data[i].in.len; j++)
            if(list_pir_block_find(&funcdef->blocks.data[i].in.data[j]->dom, blk))
                break;
        if(j >= funcdef->blocks.data[i].in.len)
            continue;

        list_pir_block_push(&blk->domfrontier, &funcdef->blocks.data[i]);
    }
}

void ir_domfrontierfunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        ir_domfrontierblk(funcdef, &funcdef->blocks.data[i]);
}

bool ir_domeq(list_pir_block_t* a, list_pir_block_t* b)
{
    int i;

    if(a->len != b->len)
        return false;

    for(i=0; i<a->len; i++)
        if(!list_pir_block_find(b, a->data[i]))
            return false;

    return true;
}

void ir_domfunc(ir_funcdef_t* funcdef)
{
    int i, j, k;
    ir_block_t *blk;

    bool change;
    list_pir_block_t newdom;

    // start node dominates itself
    list_pir_block_push(&funcdef->blocks.data[0].dom, &funcdef->blocks.data[0]);

    // for other nodes, set it at first to be dominated by every node
    for(i=1; i<funcdef->blocks.len; i++)
        for(j=0; j<funcdef->blocks.len; j++)
            list_pir_block_push(&funcdef->blocks.data[i].dom, &funcdef->blocks.data[j]);

    do
    {
        change = false;
        for(i=1, blk=funcdef->blocks.data+1; i<funcdef->blocks.len; i++, blk++)
        {
            list_pir_block_init(&newdom, 1);
            newdom.data[0] = blk;
            if(blk->in.len)
            {
                // intersection of all dom of preds
                for(j=0; j<blk->in.data[0]->dom.len; j++)
                {
                    if(blk->in.data[0]->dom.data[j] == blk)
                        continue;
                    
                    for(k=1; k<blk->in.len; k++)
                        if(!list_pir_block_find(&blk->in.data[k]->dom, blk->in.data[0]->dom.data[j]))
                            break;
                    if(k < blk->in.len)
                        continue;

                    list_pir_block_push(&newdom, blk->in.data[0]->dom.data[j]);
                }
            }

            if(ir_domeq(&newdom, &blk->dom))
            {
                list_pir_block_free(&newdom);
                continue;
            }

            list_pir_block_free(&blk->dom);
            blk->dom = newdom;
            change = true;
        }
    } while(change);
}

static set_pir_block_t visited;

static void ir_postorder_r(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;

    if(set_pir_block_contains(&visited, blk))
        return;
    set_pir_block_add(&visited, blk);

    for(i=0; i<blk->out.len; i++)
        ir_postorder_r(funcdef, blk->out.data[i]);

    list_pir_block_push(&funcdef->postorder, blk);
}

static void ir_postorderfunc(ir_funcdef_t* funcdef)
{
    list_pir_block_free(&funcdef->postorder);
    list_pir_block_init(&funcdef->postorder, 0);

    set_pir_block_alloc(&visited);
    ir_postorder_r(funcdef, &funcdef->blocks.data[0]);
    set_pir_block_free(&visited);
}

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
            ir_flowedge(blk, &funcdef->blocks.data[*map_str_u64_get(&funcdef->blktbl, inst->ternary[1].label)]);
            ir_flowedge(blk, &funcdef->blocks.data[*map_str_u64_get(&funcdef->blktbl, inst->ternary[2].label)]);
            return;
        case IR_OP_JMP:
            ir_flowedge(blk, &funcdef->blocks.data[*map_str_u64_get(&funcdef->blktbl, inst->unary.label)]);
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
    {
        ir_flowfunc(&ir.defs.data[i]);
        ir_postorderfunc(&ir.defs.data[i]);
        ir_domfunc(&ir.defs.data[i]);
        ir_domfrontierfunc(&ir.defs.data[i]);
        ir_domtreefunc(&ir.defs.data[i]);
    }
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

void ir_dumpdomtreeblk(ir_funcdef_t* func, ir_block_t* blk)
{
    int i;

    for(i=0; i<blk->domchildren.len; i++)
        printf("  %s-->%s\n", blk->name, blk->domchildren.data[i]->name);
}

void ir_dumpdomtreefunc(ir_funcdef_t* func)
{
    int i;

    printf("graph TD\n\n");

    printf("  title[\"@%s\"]\n", func->name);
    printf("  title-->%s\n", func->blocks.data[0].name);
    printf("  style title fill:#FFF,stroke:#FFF\n");
    printf("  linkStyle 0 stroke:#FFF,stroke-width:0;\n\n");

    for(i=0; i<func->blocks.len; i++)
        ir_dumpdomtreeblk(func, &func->blocks.data[i]);
}

void ir_dumpdomtree(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_dumpdomtreefunc(&ir.defs.data[i]);
}
