#include "regalloc.h"

#include <stdio.h>

static void blockusedefs(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i, j;

    set_str_t defs, uses;

    set_str_clear(&blk->regdefs);
    set_str_clear(&blk->reguses);

    for(i=0; i<blk->insts.len; i++)
    {
        ir_definedregs(&defs, &blk->insts.data[i]);
        ir_accessedregs(&uses, &blk->insts.data[i]);

        set_str_union(&blk->regdefs, &defs);
        for(j=0; j<uses.nbin; j++)
            if(uses.bins[j].state == SET_EL_FULL && !set_str_contains(&blk->regdefs, uses.bins[j].val))
                set_str_add(&blk->reguses, uses.bins[j].val);

        set_str_free(&defs);
        set_str_free(&uses);
    }
}

static void lifetimefunc(ir_funcdef_t* funcdef)
{
    int i, j;

    bool change;
    ir_block_t *blk;
    set_str_t livein, liveout;

    for(i=0; i<funcdef->blocks.len; i++)
    {
        blockusedefs(funcdef, &funcdef->blocks.data[i]);
        set_str_clear(&funcdef->blocks.data[i].livein);
        set_str_clear(&funcdef->blocks.data[i].liveout);
    }

    do
    {
        change = false;

        for(i=funcdef->postorder.len-1; i>=0; i--)
        {
            blk = funcdef->postorder.data[i];

            set_str_dup(&livein, &blk->livein);
            set_str_dup(&liveout, &blk->liveout);

            // out = union of successor livein
            set_str_clear(&blk->liveout);
            for(j=0; j<blk->out.len; j++)
                set_str_union(&blk->liveout, &blk->out.data[j]->livein);

            // livein = used u (liveout - defined)
            set_str_free(&blk->livein);
            set_str_dup(&blk->livein, &blk->reguses);
            for(j=0; j<blk->liveout.nbin; j++)
            {
                if(blk->liveout.bins[j].state != SET_EL_FULL)
                    continue;
                if(set_str_contains(&blk->regdefs, blk->liveout.bins[j].val))
                    continue;
                set_str_add(&blk->livein, blk->liveout.bins[j].val);
            }

            if(!set_str_isequal(&livein, &blk->livein) || !set_str_isequal(&liveout, &blk->liveout))
                change = true;

            set_str_free(&livein);
            set_str_free(&liveout);
        }
    } while(change);
}

void reglifetime(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        lifetimefunc(&ir.defs.data[i]);

    exit(0);
}