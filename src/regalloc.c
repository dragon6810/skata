#include "regalloc.h"

#include <assert.h>
#include <stdio.h>

int nreg = 8;

void regalloc_colorreg(ir_funcdef_t* funcdef, ir_reg_t* reg)
{
    int i;

    bool openreg[nreg];
    int hardreg;

    for(i=0; i<nreg; i++)
        openreg[i] = true;

    // greedy colorer for now
    for(i=0; i<reg->interfere.nbin; i++)
    {
        if(reg->interfere.bins[i].state != SET_EL_FULL)
            continue;
        hardreg = map_str_ir_reg_get(&funcdef->regs, &reg->interfere.bins[i].val)->hardreg;

        if(hardreg >= 0)
            openreg[hardreg] = false;
    }
    
    for(i=0; i<nreg; i++)
    {
        if(!openreg[i])
            continue;
        reg->hardreg = i;
        return;
    }

    assert(0 && "TODO: spilling");
}

void regalloc_color(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
        if(funcdef->regs.bins[i].state == MAP_EL_FULL)
            regalloc_colorreg(funcdef, &funcdef->regs.bins[i].val);
}

void regalloc_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
    {
        if(funcdef->regs.bins[i].state != MAP_EL_FULL)
            continue;
        funcdef->regs.bins[i].val.hardreg = -1;
    }

    regalloc_color(funcdef);
}

void regalloc(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        regalloc_funcdef(&ir.defs.data[i]);
}

static set_str_t printededges;

static char* makeedgekey(const char* a, const char* b)
{
    const char *l, *r, *temp;
    int len;
    char *key;

    l = a;
    r = b;

    if (strcmp(l, r) > 0) 
    {
        temp = l;
        l = r;
        r = temp;
    }

    len = strlen(l) + 1 + strlen(r) + 1;
    key = malloc(len);
    snprintf(key, len, "%s|%s", l, r);
    
    return key;
}

static void dumpregedges(ir_reg_t* reg)
{
    int i;

    char *edgekey;

    printf("  %%%s(%%%s)\n", reg->name, reg->name);
    for(i=0; i<reg->interfere.nbin; i++)
    {
        if(reg->interfere.bins[i].state != SET_EL_FULL)
            continue;

        edgekey = makeedgekey(reg->name, reg->interfere.bins[i].val);
        if(set_str_contains(&printededges, edgekey))
        {
            free(edgekey);
            continue;
        }

        set_str_add(&printededges, edgekey);
        free(edgekey);

        printf("  %%%s --- %%%s\n", reg->name, reg->interfere.bins[i].val);
    }
}

static void dumpfuncreggraph(ir_funcdef_t* funcdef)
{
    int i;

    printf("graph\n");

    set_str_alloc(&printededges);
    for(i=0; i<funcdef->regs.nbin; i++)
    {
        if(funcdef->regs.bins[i].state != MAP_EL_FULL)
            continue;
        dumpregedges(&funcdef->regs.bins[i].val);
    }
    set_str_free(&printededges);
}

void dumpreggraph(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        dumpfuncreggraph(&ir.defs.data[i]);
}