#include "ir.h"

#include <stdio.h>

LIST_DEF(pir_block)
LIST_DEF_FREE(pir_block)

ir_block_t* ir_idom(ir_block_t* blk)
{
    int i, j;

    ir_block_t *idom;

    for(i=0; i<blk->dom.nbin; i++)
    {
        if(blk->dom.bins[i].state != SET_EL_FULL)
            continue;
        
        idom = blk->dom.bins[i].val;
        if(idom == blk)
            continue;

        for(j=0; j<blk->dom.nbin; j++)
        {
            if(i == j || blk->dom.bins[j].state != SET_EL_FULL || blk->dom.bins[j].val == blk)
                continue;
            if(set_pir_block_contains(&blk->dom.bins[j].val->dom, idom))
                break;
        }

        if(j < blk->dom.nbin)
            continue;

        return idom;
    }

    return NULL;
}

void ir_domtreefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
    {
        funcdef->blocks.data[i].idom = NULL;
        list_pir_block_clear(&funcdef->blocks.data[i].domchildren);
    }

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

    ir_block_t *df;

    for(i=0, df=funcdef->blocks.data; i<funcdef->blocks.len; i++, df++)
    {
        if(df == blk)
            continue;
        if(set_pir_block_contains(&df->dom, blk))
            continue;

        for(j=0; j<df->in.len; j++)
            if(set_pir_block_contains(&df->in.data[j]->dom, blk))
                break;
        if(j >= df->in.len)
            continue;

        list_pir_block_push(&blk->domfrontier, df);
    }
}

void ir_domfrontierfunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        list_pir_block_clear(&funcdef->blocks.data[i].domfrontier);

    for(i=0; i<funcdef->blocks.len; i++)
        ir_domfrontierblk(funcdef, &funcdef->blocks.data[i]);
}

void ir_domfunc(ir_funcdef_t* funcdef)
{
    int i, j;
    ir_block_t *blk;

    bool change;
    set_pir_block_t newdom;

    for(i=0; i<funcdef->blocks.len; i++)
        set_pir_block_clear(&funcdef->blocks.data[i].dom);

    // start node dominates itself
    set_pir_block_add(&funcdef->blocks.data[0].dom, &funcdef->blocks.data[0]);

    // for other nodes, set it at first to be dominated by every node
    for(i=1; i<funcdef->blocks.len; i++)
        for(j=0; j<funcdef->blocks.len; j++)
            set_pir_block_add(&funcdef->blocks.data[i].dom, &funcdef->blocks.data[j]);

    set_pir_block_alloc(&newdom);

    do
    {
        change = false;
        for(i=1, blk=&funcdef->blocks.data[1]; i<funcdef->blocks.len; i++, blk++)
        {
            set_pir_block_clear(&newdom);
            
            if(blk->in.len)
            {
                // intersection of all dom of preds
                set_pir_block_union(&newdom, &blk->in.data[0]->dom);
                for(j=1; j<blk->in.len; j++)
                    set_pir_block_intersection(&newdom, &blk->in.data[j]->dom);
            }

            // dominates itself
            set_pir_block_add(&newdom, blk);

            if(set_pir_block_isequal(&newdom, &blk->dom))
                continue;

            set_pir_block_free(&blk->dom);
            set_pir_block_dup(&blk->dom, &newdom);
            change = true;
        }
    } while(change);

    set_pir_block_free(&newdom);
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
    list_pir_block_clear(&funcdef->postorder);

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
    {
        list_pir_block_clear(&funcdef->blocks.data[i].in);
        list_pir_block_clear(&funcdef->blocks.data[i].out);
    }

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
