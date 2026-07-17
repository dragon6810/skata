#include "back.h"

#include "middle/ir.h"

map_u64_u64_t typereductions;

static ir_primitive_e back_tryreduce(ir_primitive_e type)
{
    uint64_t *newtype;

    newtype = map_u64_u64_get(&typereductions, type);
    if(!newtype)
        return type;
    return *newtype;
}

static void back_reduceblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int o;
    ir_inst_t *inst;
    ir_operand_t *operand;

    list_pir_operand_t operands;

    for(inst=blk->insts; inst; inst=inst->next)
    {
        ir_instoperands(&operands, inst);

        for(o=0; o<operands.len; o++)
        {
            operand = operands.data[o];
            if(operand->type == IR_OPERAND_LIT)
                operand->literal.type = back_tryreduce(operand->literal.type);
        }

        list_pir_operand_free(&operands);
    }
}

static void back_reducefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
        if(funcdef->regs.bins[i].state == MAP_EL_FULL)
            funcdef->regs.bins[i].val.type = back_tryreduce(funcdef->regs.bins[i].val.type);

    for(i=0; i<funcdef->blocks.len; i++)
        back_reduceblk(funcdef, &funcdef->blocks.data[i]);
}

void back_typereduction(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        back_reducefunc(&ir.defs.data[i]);
}

// NOTE: all alignment stuff depends on each primitive type size being divisible by any smaller primitive type size
// im not sure if this is true on every arch

int back_aggbytesize(uint64_t aggid)
{
    ir_aggregate_t *agg;
    int size;
    list_ir_fid_t fidlist;

    agg = map_u64_ir_aggregate_get(&ir.aggs, aggid);
    assert(agg);
    assert(agg->type == AGG_STRUCT);
    list_ir_fid_init(&fidlist, 1);
    fidlist.data[0].fid = agg->struc.fids.len;
    fidlist.data[0].typeid = 0;
    size = back_fidoffset(aggid, &fidlist);
    list_ir_fid_free(&fidlist);

    return size;
}

static int back_structalignment(uint64_t aggid)
{
    int i, j;
    ir_type_t *type;

    ir_aggregate_t *agg;
    int biggest, elsize;

    agg = map_u64_ir_aggregate_get(&ir.aggs, aggid);
    assert(agg);
    assert(agg->type == AGG_STRUCT);

    biggest = 0;
    for(i=0; i<agg->struc.fids.len; i++)
    {
        for(j=0, type=agg->struc.fids.data[i].types.data; j<agg->struc.fids.data[i].types.len; j++, type++)
        {
            switch(type->type)
            {
            case IR_TYPE_PRIM:
                elsize = ir_primbytesize(type->prim);
                break;
            case IR_TYPE_AGG:
                elsize = back_structalignment(type->agg);
                break;
            default:
                assert(0);
                break;
            }

            if(elsize > biggest)
                biggest = elsize;
        }
    }

    return biggest;
}

int back_alignup(int val, int align)
{
    if(align <= 0)
        return val;
    return (val + align - 1) / align * align;
}

static int back_nextprimsize(uint64_t aggid, int nextfid)
{
    int i;
    ir_type_t *type;

    ir_aggregate_t* agg;
    int biggest, elsize;

    agg = map_u64_ir_aggregate_get(&ir.aggs, aggid);
    assert(agg);
    assert(agg->type == AGG_STRUCT);
    assert(nextfid < agg->struc.fids.len);

    for(i=biggest=0, type=agg->struc.fids.data[nextfid].types.data; i<agg->struc.fids.data[nextfid].types.len; i++, type++)
    {
        switch(type->type)
        {
        case IR_TYPE_PRIM:
            elsize = ir_primbytesize(type->prim);
            break;
        case IR_TYPE_AGG:
            elsize = back_structalignment(type->agg);
            break;
        default:
            assert(0);
            break;
        }

        if(elsize > biggest)
            biggest = elsize;
    }

    return biggest;
}

int back_typealignment(const ir_type_t* type)
{
    switch(type->type)
    {
    case IR_TYPE_PRIM:
        return ir_primbytesize(type->prim);
    case IR_TYPE_AGG:
        return back_structalignment(type->agg);
    default:
        assert(0);
        return 0;
    }
}

int back_typebytesize(const ir_type_t* type)
{
    switch(type->type)
    {
    case IR_TYPE_PRIM:
        return ir_primbytesize(type->prim);
    case IR_TYPE_AGG:
        return back_aggbytesize(type->agg);
    default:
        assert(0);
        return 0;
    }
}

int back_fidoffset(uint64_t aggid, list_ir_fid_t* fids)
{
    int i, j, k;

    ir_aggregate_t *agg;
    int size, elsize, typesize;
    uint64_t curaggid;

    size = 0;

    curaggid = aggid;
    agg = map_u64_ir_aggregate_get(&ir.aggs, curaggid);
    assert(agg);
    assert(agg->type == AGG_STRUCT);
    for(i=0; i<fids->len; i++)
    {
        for(j=0; j<fids->data[i].fid; j++)
        {
            elsize = 0;
            for(k=0; k<agg->struc.fids.data[j].types.len; k++)
            {
                typesize = back_typebytesize(&agg->struc.fids.data[j].types.data[k]);
                if(typesize > elsize)
                    elsize = typesize;
            }
            size += elsize;

            // pad this element up to the alignment of the next element,
            // or to the whole struct's alignment when it's the last one
            if(j+1 < agg->struc.fids.len)
                size = back_alignup(size, back_nextprimsize(curaggid, j+1));
            else
                size = back_alignup(size, back_structalignment(curaggid));
        }
        if(i<fids->len-1)
        {
            curaggid = agg->struc.fids.data[fids->data[i].fid].types.data[fids->data[i].typeid].agg;
            agg = map_u64_ir_aggregate_get(&ir.aggs, curaggid);
        }
    }
    return size;
}

static void back_instfie(ir_funcdef_t* func, ir_block_t* blk, ir_inst_t* last, ir_inst_t* inst)
{
    int i;

    list_pir_operand_t operands;
    ir_reg_t *reg;
    ir_inst_t *fie;

    ir_instoperands(&operands, inst);

    if(inst->op == IR_OP_STORE || inst->op == IR_OP_STOREFID || inst->op == IR_OP_ALLOCA)
        list_pir_operand_remove(&operands, 0);
    if(inst->op == IR_OP_LOAD || inst->op == IR_OP_LOADFID)
        list_pir_operand_remove(&operands, 1);

    for(i=0; i<operands.len; i++)
    {
        if(operands.data[i]->type != IR_OPERAND_REG)
            continue;
        reg = map_str_ir_reg_get(&func->regs, operands.data[i]->reg.name);
        assert(reg);
        if(!reg->def || reg->def->op != IR_OP_ALLOCA)
            continue;
        
        fie = malloc(sizeof(ir_inst_t));
        fie->op = IR_OP_FIE;
        fie->binary[0].type = IR_OPERAND_REG;
        fie->binary[0].reg.name = ir_allocreg(func, IR_PRIM_PTR);
        fie->binary[1].type = IR_OPERAND_REG;
        fie->binary[1].reg.name = operands.data[i]->reg.name;
        fie->next = inst;
        if(last)
            last->next = fie;
        else
            blk->insts = fie;
        last = fie;

        operands.data[i]->reg.name = strdup(fie->binary[0].reg.name);
    }
    
    list_pir_operand_free(&operands);
}

static void back_fieblk(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    ir_inst_t *inst, *last;

    for(last=NULL, inst=blk->insts; inst; last=inst, inst=inst->next)
        back_instfie(funcdef, blk, last, inst);
}

static void back_fiefunc(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->blocks.len; i++)
        back_fieblk(funcdef, &funcdef->blocks.data[i]);
}

void back_fie(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        back_fiefunc(&ir.defs.data[i]);
}