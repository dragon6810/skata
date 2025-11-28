#include "regalloc.h"

#include <stdio.h>

static void regalloc_cpyregspan(ir_regspan_t* dst, ir_regspan_t* src)
{
    dst->reg = strdup(src->reg);
    dst->span[0] = src->span[0];
    dst->span[1] = src->span[1];
}

static void regalloc_freeregspan(ir_regspan_t* regspan)
{
    free(regspan->reg);
}

LIST_DEF(ir_regspan)
LIST_DEF_FREE_DECONSTRUCT(ir_regspan, regalloc_freeregspan)
MAP_DEF(char*, ir_regspan_t, str, ir_regspan, 
        hash_str, map_strcmp, map_strcpy, regalloc_cpyregspan, map_freestr, regalloc_freeregspan)

static void interference(ir_funcdef_t* funcdef, ir_block_t* block)
{
    int i, j;
    ir_regspan_t *ispan, *jspan;

    ir_reg_t *reg;

    for(i=0, ispan=block->spans.data; i<block->spans.len; i++, ispan++)
    {
        for(j=i+1, jspan=block->spans.data+i+1; j<block->spans.len; j++, jspan++)
        {
            if(!strcmp(ispan->reg, jspan->reg))
                continue;
            if(ispan->span[1] <= jspan->span[0])
                continue;
            if(ispan->span[0] >= jspan->span[1])
                continue;
            
            reg = map_str_ir_reg_get(&funcdef->regs, &ispan->reg);
            set_str_add(&reg->interfere, jspan->reg);
            reg = map_str_ir_reg_get(&funcdef->regs, &jspan->reg);
            set_str_add(&reg->interfere, ispan->reg);
        }
    }
}

static void blockspans(ir_block_t* block)
{
    int i, j;
    ir_inst_t *inst;

    map_str_ir_regspan_t curspans;
    ir_regspan_t span, *pspan;
    set_str_t defs, used;

    list_ir_regspan_clear(&block->spans);
    map_str_ir_regspan_alloc(&curspans);

    for(i=0; i<block->livein.nbin; i++)
    {
        if(block->livein.bins[i].state != SET_EL_FULL)
            continue;
        span.reg = block->livein.bins[i].val;
        span.span[0] = span.span[1] = 0;
        map_str_ir_regspan_set(&curspans, &span.reg, &span);
    }

    for(i=0, inst=block->insts.data; i<block->insts.len; i++, inst++)
    {
        ir_definedregs(&defs, inst);
        ir_accessedregs(&used, inst);

        // extend spans of accessed registers
        for(j=0; j<used.nbin; j++)
        {
            if(used.bins[j].state != SET_EL_FULL)
                continue;

            pspan = map_str_ir_regspan_get(&curspans, &used.bins[j].val);
            if(!pspan)
            {
                printf("inst opcode: %d\n", (int) inst->op);
                fprintf(stderr, "use of undefined register %s\n", used.bins[j].val);
                exit(1);
            }

            pspan->span[1] = i + 1;
        }

        // finish spans of defined registers and start new ones
        for(j=0; j<defs.nbin; j++)
        {
            if(defs.bins[j].state != SET_EL_FULL)
                continue;
            
            pspan = map_str_ir_regspan_get(&curspans, &defs.bins[j].val);
            if(pspan)
            {
                span = *pspan;
                span.reg = strdup(span.reg);
                list_ir_regspan_ppush(&block->spans, &span);
            }

            span.reg = defs.bins[j].val;
            span.span[0] = span.span[1] = i;
            map_str_ir_regspan_set(&curspans, &span.reg, &span);
        }

        set_str_free(&defs);
        set_str_free(&used);
    }

    // on any remaining spans, extend them to the end of the block & add them
    // if they are alive after the block, extend them to the end
    for(j=0; j<curspans.nbin; j++)
    {
        if(curspans.bins[j].state != MAP_EL_FULL)
            continue;
        
        span = curspans.bins[j].val;
        span.reg = strdup(span.reg);
        if(set_str_contains(&block->liveout, curspans.bins[j].key))
            span.span[1] = i;
        list_ir_regspan_ppush(&block->spans, &span);
    }

    map_str_ir_regspan_free(&curspans);
}

static void funcinouts(ir_funcdef_t* funcdef)
{
    int i, j;

    bool change;
    ir_block_t *blk;
    set_str_t livein, liveout;

    for(i=0; i<funcdef->blocks.len; i++)
    {
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

        for(j=0; j<uses.nbin; j++)
            if(uses.bins[j].state == SET_EL_FULL && !set_str_contains(&blk->regdefs, uses.bins[j].val))
                set_str_add(&blk->reguses, uses.bins[j].val);
        set_str_union(&blk->regdefs, &defs);

        set_str_free(&defs);
        set_str_free(&uses);
    }
}

static void lifetimefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        blockusedefs(funcdef, &funcdef->blocks.data[i]);
    funcinouts(funcdef);
    for(i=0; i<funcdef->blocks.len; i++)
    {
        blockspans(&funcdef->blocks.data[i]);
        interference(funcdef, &funcdef->blocks.data[i]);
    }
}

void reglifetime(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        lifetimefunc(&ir.defs.data[i]);
}