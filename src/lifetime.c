#include "regalloc.h"

#include <stdio.h>

static void blockusedefs(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;

    set_str_t defs, uses;

    for(i=0; i<blk->insts.len; i++)
    {
        ir_definedregs(&defs, &blk->insts.data[i]);
        ir_accessedregs(&uses, &blk->insts.data[i]);

        set_str_union(&blk->regdefs, &defs);
        set_str_union(&blk->reguses, &uses);

        set_str_free(&defs);
        set_str_free(&uses);
    }

    printf("%s defs = { ", blk->name);
    for(i=0; i<blk->regdefs.nbin; i++)
        if(blk->regdefs.bins[i].state == SET_EL_FULL)
            printf("%s ", blk->regdefs.bins[i].val);
    printf("}\n");

    printf("%s uses = { ", blk->name);
    for(i=0; i<blk->reguses.nbin; i++)
        if(blk->reguses.bins[i].state == SET_EL_FULL)
            printf("%s ", blk->reguses.bins[i].val);
    printf("}\n");
}

static void lifetimefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        blockusedefs(funcdef, &funcdef->blocks.data[i]);
}

void reglifetime(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        lifetimefunc(&ir.defs.data[i]);

    exit(0);
}