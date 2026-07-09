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

// YOU are responsible for the returned string
static char* regalloc_newslot(ir_funcdef_t* funcdef, ir_primitive_e prim)
{
    ir_inst_t *inst;
    ir_reg_t reg;
    char name[16];
    ir_block_t *entry;

    entry = funcdef->blocks.data;
    assert(entry);

    memset(&reg, 0, sizeof(reg));

    snprintf(name, 16, "%llu", (unsigned long long) funcdef->ntempreg++);
    reg.name = strdup(name);
    reg.type = prim;
    reg.virtual = true;
    set_str_alloc(&reg.interfere);
    map_str_ir_reg_set(&funcdef->regs, reg.name, reg);
    set_str_free(&reg.interfere);
    
    inst = malloc(sizeof(ir_inst_t));
    inst->op = IR_OP_ALLOCA;
    inst->unary.type = IR_OPERAND_REG;
    inst->unary.reg.name = strdup(name);
    inst->alloca.type.type = IR_TYPE_PRIM;
    inst->alloca.type.prim = prim;
    inst->next = entry->insts;
    entry->insts = inst;

    return reg.name;
}

// insert newinst at the end of blk, before its branch if it has one
static void regalloc_insertend(ir_block_t* blk, ir_inst_t* newinst)
{
    ir_inst_t *cur;

    newinst->next = NULL;

    if(!blk->insts)
    {
        blk->insts = newinst;
        return;
    }
    if(blk->insts == blk->branch)
    {
        newinst->next = blk->insts;
        blk->insts = newinst;
        return;
    }

    for(cur=blk->insts; cur->next && cur->next != blk->branch; cur=cur->next);
    newinst->next = cur->next;
    cur->next = newinst;
}

static ir_block_t* regalloc_labelblock(ir_funcdef_t* funcdef, const char* label)
{
    uint64_t *idx;

    idx = map_str_u64_get(&funcdef->blktbl, (char*) label);
    assert(idx);
    return &funcdef->blocks.data[*idx];
}

static void regalloc_deleteinst(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    ir_block_t *blk;
    ir_inst_t *cur;

    for(blk=funcdef->blocks.data; blk<&funcdef->blocks.data[funcdef->blocks.len]; blk++)
    {
        if(blk->insts == inst)
        {
            blk->insts = inst->next;
            inst->next = NULL;
            ir_instfree(inst);
            return;
        }

        for(cur=blk->insts; cur; cur=cur->next)
        {
            if(cur->next != inst)
                continue;
            cur->next = inst->next;
            inst->next = NULL;
            ir_instfree(inst);
            return;
        }
    }

    assert(0);
}

static ir_inst_t* regalloc_newload(ir_funcdef_t* funcdef, char* dst, const char* slotname)
{
    ir_inst_t *inst;

    inst = calloc(1, sizeof(ir_inst_t));
    inst->op = IR_OP_LOAD;
    inst->binary[0].type = IR_OPERAND_REG;
    inst->binary[0].reg.name = dst;
    inst->binary[1].type = IR_OPERAND_REG;
    inst->binary[1].reg.name = strdup(slotname);

    return inst;
}

// a phi's value is established on the incoming edges, so the spill stores
// go at the end of each predecessor and the phi itself disappears
static void regalloc_spillphidef(ir_funcdef_t* funcdef, const char* regname, const char* slotname, ir_inst_t* phi)
{
    int i;

    ir_block_t *pred;
    ir_inst_t *newinst;
    ir_operand_t *operand;

    for(i=1; i+1<phi->variadic.len; i+=2)
    {
        operand = &phi->variadic.data[i+1];

        if(operand->type == IR_OPERAND_REG && !strcmp(operand->reg.name, regname))
            continue;

        newinst = calloc(1, sizeof(ir_inst_t));
        newinst->op = IR_OP_STORE;
        newinst->binary[0].type = IR_OPERAND_REG;
        newinst->binary[0].reg.name = strdup(slotname);
        ir_cpyoperand(&newinst->binary[1], operand);

        pred = regalloc_labelblock(funcdef, phi->variadic.data[i].label);
        regalloc_insertend(pred, newinst);
    }

    regalloc_deleteinst(funcdef, phi);
}

static void regalloc_spillreg(ir_funcdef_t* funcdef, const char* victim)
{
    int i;
    ir_block_t *blk, *pred;
    ir_inst_t *inst;

    char *regname;
    char *slotname;
    char *newreg;
    ir_reg_t *oldreg;
    ir_primitive_e primtype;
    ir_inst_t *def;
    ir_inst_t *newinst;
    set_str_t accessed;
    list_pir_operand_t operands;
    ir_operand_t *operand;

    // inserting registers can move (and removing can free) map entries,
    // so keep private copies of everything about the victim
    regname = strdup(victim);
    oldreg = map_str_ir_reg_get(&funcdef->regs, regname);
    assert(oldreg);
    primtype = oldreg->type;
    def = oldreg->def;
    assert(def);

    slotname = regalloc_newslot(funcdef, primtype);

    // rewrite the uses before inserting any def stores: the def stores read
    // the victim (or, for phis, write its next value at the pred edge), so
    // they must not be visited as uses, and block-end reloads have to land
    // before block-end stores
    for(blk=funcdef->blocks.data; blk<&funcdef->blocks.data[funcdef->blocks.len]; blk++)
    {
        for(inst=blk->insts; inst; inst=inst->next)
        {
            if(inst == def)
                continue;

            ir_accessedregs(&accessed, inst);
            if(!set_str_contains(&accessed, regname))
            {
                set_str_free(&accessed);
                continue;
            }
            set_str_free(&accessed);

            // a phi reads its operand on the incoming edge, so the reload
            // belongs at the end of the matching predecessor
            if(inst->op == IR_OP_PHI)
            {
                for(i=1; i+1<inst->variadic.len; i+=2)
                {
                    operand = &inst->variadic.data[i+1];
                    if(operand->type != IR_OPERAND_REG)
                        continue;
                    if(strcmp(operand->reg.name, regname))
                        continue;

                    newreg = ir_allocreg(funcdef, primtype);
                    map_str_ir_reg_get(&funcdef->regs, newreg)->nospill = true;

                    pred = regalloc_labelblock(funcdef, inst->variadic.data[i].label);
                    regalloc_insertend(pred, regalloc_newload(funcdef, newreg, slotname));

                    free(operand->reg.name);
                    operand->reg.name = strdup(newreg);
                }
                continue;
            }

            newreg = ir_allocreg(funcdef, primtype);
            map_str_ir_reg_get(&funcdef->regs, newreg)->nospill = true;

            newinst = malloc(sizeof(ir_inst_t));
            memcpy(newinst, inst, sizeof(ir_inst_t));

            inst->op = IR_OP_LOAD;
            inst->binary[0].type = IR_OPERAND_REG;
            inst->binary[0].reg.name = newreg;
            inst->binary[1].type = IR_OPERAND_REG;
            inst->binary[1].reg.name = strdup(slotname);
            inst->next = newinst;

            if(blk->branch == inst)
                blk->branch = newinst;
            if(def == inst)
                def = newinst;

            ir_instoperands(&operands, newinst);
            for(i=0; i<operands.len; i++)
            {
                operand = operands.data[i];
                if(operand->type != IR_OPERAND_REG)
                    continue;
                if(!operand->reg.name || strcmp(operand->reg.name, regname))
                    continue;
                free(operand->reg.name);
                operand->reg.name = strdup(newreg);
            }
            list_pir_operand_free(&operands);

            inst = newinst;
        }
    }

    if(def->op == IR_OP_PHI)
    {
        regalloc_spillphidef(funcdef, regname, slotname, def);
        map_str_ir_reg_remove(&funcdef->regs, regname);
    }
    else
    {
        newinst = calloc(1, sizeof(ir_inst_t));
        newinst->op = IR_OP_STORE;
        newinst->binary[0].type = IR_OPERAND_REG;
        newinst->binary[0].reg.name = strdup(slotname);
        newinst->binary[1].type = IR_OPERAND_REG;
        newinst->binary[1].reg.name = strdup(regname);
        newinst->next = def->next;
        def->next = newinst;
    }

    free(regname);
    free(slotname);
}

static ir_reg_t* regalloc_pickvictim(ir_funcdef_t* funcdef, set_pir_reg_t* set)
{
    int i;

    for(i=0; i<set->nbin; i++)
    {
        if(set->bins[i].state != SET_EL_FULL)
            continue;
        if(set->bins[i].val->nospill || set->bins[i].val->virtual)
            continue;
        return set->bins[i].val;
    }

    return NULL;
}

bool regalloc_colorreg(ir_funcdef_t* funcdef, ir_reg_t* reg)
{
    int i;

    set_phardreg_t openreg;
    ir_reg_t *other;

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

        other = map_str_ir_reg_get(&funcdef->regs, reg->interfere.bins[i].val);
        if(!other)
            continue;

        set_phardreg_remove(&openreg, other->hardreg);
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
        map_str_ir_reg_get(&funcdef->regs, param->reg)->nospill = true;
        map_str_ir_reg_get(&funcdef->regs, param->reg)->hardreg = &regpool.data[i];
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
        if(funcdef->regs.bins[i].val.virtual)
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
    assert(reg && "every uncolorable register is unspillable");
    regalloc_spillreg(funcdef, reg->name);

    ir_regdefs();
    reglifetime();
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