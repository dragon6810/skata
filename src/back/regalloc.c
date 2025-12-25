#include "regalloc.h"

#include <assert.h>
#include <stdio.h>

uint64_t hash_hardreg(hardreg_t* val)
{
    return hash_str(&val->name) * 3 + val->flags;
}

bool cmp_hardreg(hardreg_t* a, hardreg_t* b)
{
    return a->flags == b->flags && !strcmp(a->name, b->name);
}

void cpy_hardreg(hardreg_t* dst, hardreg_t* src)
{
    dst->flags = src->flags;
    dst->name = strdup(src->name);
    map_u64_str_dup(&dst->names, &src->names);
}

void free_hardreg(hardreg_t* val)
{
    free(val->name);
    map_u64_str_free(&val->names);
}

uint64_t hash_phardreg(hardreg_t** val)
{
    return (uint64_t) *val;
}

LIST_DEF(hardreg)
LIST_DEF_FREE_DECONSTRUCT(hardreg, free_hardreg)
LIST_DEF(phardreg)
LIST_DEF_FREE(phardreg)
SET_DEF(hardreg_t, hardreg, hash_hardreg, cmp_hardreg, cpy_hardreg, free_hardreg)
SET_DEF(hardreg_t*, phardreg, hash_phardreg, NULL, NULL, NULL)

list_hardreg_t regpool;
set_phardreg_t calleepool;
set_phardreg_t callerpool;
set_phardreg_t scratchpool;
list_phardreg_t scratchlist;
list_phardreg_t parampool;
list_phardreg_t retpool;

static uint64_t regalloc_hashpreg(ir_reg_t** val)
{
    return (uint64_t) *val;
}

SET_DEF(ir_reg_t*, pir_reg, regalloc_hashpreg, NULL, NULL, NULL)

void regalloc_init(void)
{
    int i;
    hardreg_t *reg;

    set_phardreg_alloc(&calleepool);
    set_phardreg_alloc(&callerpool);
    set_phardreg_alloc(&scratchpool);
    list_phardreg_init(&scratchlist, 0);
    list_phardreg_init(&parampool, 0);
    list_phardreg_init(&retpool, 0);

    for(i=0, reg=regpool.data; i<regpool.len; i++, reg++)
    {
        if(reg->flags & HARDREG_CALLER)
            set_phardreg_add(&callerpool, reg);
        else
            set_phardreg_add(&calleepool, reg);

        if(reg->flags & HARDREG_SCRATCH)
        {
            set_phardreg_add(&scratchpool, reg);
            list_phardreg_push(&scratchlist, reg);
        }

        if(reg->flags & HARDREG_PARAM)
            list_phardreg_push(&parampool, reg);

        if(reg->flags & HARDREG_RETURN)
            list_phardreg_push(&retpool, reg);
    }
}

static void regalloc_spillreg(ir_funcdef_t* funcdef, char* regname)
{
    assert(0);
}

static ir_reg_t* regalloc_pickvictim(ir_funcdef_t* funcdef, set_pir_reg_t* set)
{
    int i;

    for(i=0; i<set->nbin; i++)
        if(set->bins[i].state == SET_EL_FULL)
            return set->bins[i].val;

    return NULL;
}

bool regalloc_colorreg(ir_funcdef_t* funcdef, ir_reg_t* reg)
{
    int i;

    set_phardreg_t openreg;
    hardreg_t *hardreg;

    if(reg->hardreg)
        return true;

    set_phardreg_alloc(&openreg);
    for(i=0; i<regpool.len; i++)
        set_phardreg_add(&openreg, &regpool.data[i]);

    set_phardreg_subtract(&openreg, &scratchpool);

    // greedy colorer for now
    for(i=0; i<reg->interfere.nbin; i++)
    {
        if(reg->interfere.bins[i].state != SET_EL_FULL)
            continue;

        hardreg = map_str_ir_reg_get(&funcdef->regs, reg->interfere.bins[i].val)->hardreg;
        set_phardreg_remove(&openreg, hardreg);
    }
    
    for(i=0; i<openreg.nbin; i++)
    {
        if(openreg.bins[i].state != SET_EL_FULL)
            continue;

        reg->hardreg = openreg.bins[i].val;
        set_phardreg_free(&openreg);
        return true;
    }

    set_phardreg_free(&openreg);
    return false;
}

void regalloc_colorparams(ir_funcdef_t* funcdef)
{
    int i;
    ir_param_t *param;

    for(i=0, param=funcdef->params.data; i<funcdef->params.len && i<regpool.len; i++, param++)
    {
        if(param->loc.type != IR_LOCATION_REG)
            continue;

        map_str_ir_reg_get(&funcdef->regs, param->loc.reg)->hardreg = &regpool.data[i];
    }
}

void regalloc_color(ir_funcdef_t* funcdef)
{
    int i;

    set_pir_reg_t failed;
    ir_reg_t *reg;

try:
    set_pir_reg_alloc(&failed);

    for(i=0; i<funcdef->regs.nbin; i++)
        if(funcdef->regs.bins[i].state == MAP_EL_FULL)
            funcdef->regs.bins[i].val.hardreg = NULL;

    regalloc_colorparams(funcdef);
    for(i=0; i<funcdef->regs.nbin; i++)
    {
        if(funcdef->regs.bins[i].state != MAP_EL_FULL)
            continue;
            
        if(!regalloc_colorreg(funcdef, &funcdef->regs.bins[i].val))
            set_pir_reg_add(&failed, &funcdef->regs.bins[i].val);
    }

    if(!failed.nfull)
    {
        set_pir_reg_free(&failed);
        return;
    }

    reg = regalloc_pickvictim(funcdef, &failed);
    regalloc_spillreg(funcdef, reg->name);

    set_pir_reg_free(&failed);
    goto try;
}

void regalloc_funcdef(ir_funcdef_t* funcdef)
{
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

    printf("  %%%s(%%%s - %s)\n", reg->name, reg->name, reg->hardreg->name);
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