#include "ir.h"

#include <stdio.h>

// copies ownership of operands
static void ir_insertcpy(ir_block_t* blk, ir_copy_t* cpy)
{
    uint64_t idx;
    ir_inst_t inst;

    idx = blk->insts.len;
    if(blk->insts.len 
    && (blk->insts.data[blk->insts.len-1].op == IR_OP_BR || blk->insts.data[blk->insts.len-1].op == IR_OP_JMP))
        idx--;

    if(cpy->dst.type == IR_OPERAND_REG && cpy->src.type != IR_OPERAND_VAR)
        inst.op = IR_OP_MOVE;
    else if(cpy->dst.type == IR_OPERAND_REG && cpy->src.type == IR_OPERAND_VAR)
        inst.op = IR_OP_LOAD;
    else if(cpy->dst.type == IR_OPERAND_VAR && cpy->src.type == IR_OPERAND_REG)
        inst.op = IR_OP_STORE;
    else
        assert(0);

    ir_cpyoperand(&inst.binary[0], &cpy->dst);
    ir_cpyoperand(&inst.binary[1], &cpy->src);

    list_ir_inst_insert(&blk->insts, idx, inst);
}

// returns true if dst is not used as any other src
static bool ir_iscpyacyclic(ir_funcdef_t* funcdef, ir_block_t* blk, uint64_t idx)
{
    int i;

    for(i=0; i<blk->phicpys.len; i++)
    {
        if(idx == i)
            continue;

        if(ir_operandeq(funcdef, &blk->phicpys.data[i].src, &blk->phicpys.data[idx].dst))
            return false;
    }

    return true;
}

static void ir_sequentializecpys(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;

    // weed out acyclic copies
    for(i=0; i<blk->phicpys.len; i++)
    {
        if(!ir_iscpyacyclic(funcdef, blk, i))
            continue;

        ir_insertcpy(blk, &blk->phicpys.data[i]);
        list_ir_copy_remove(&blk->phicpys, i);
        i--;
    }

    // TODO: resolve cyclic graphs
    assert(!blk->phicpys.len);
}

static void ir_findphicpys(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    int i;
    ir_copy_t cpy;

    uint64_t *ilabel;
    ir_block_t *blk;

    assert(inst->op == IR_OP_PHI);

    for(i=2; i<inst->variadic.len; i+=2)
    {
        ir_cpyoperand(&cpy.dst, &inst->variadic.data[0]);
        ir_cpyoperand(&cpy.src, &inst->variadic.data[i]);

        ilabel = map_str_u64_get(&funcdef->blktbl, inst->variadic.data[i-1].label);
        assert(ilabel);
        blk = &funcdef->blocks.data[*ilabel];

        list_ir_copy_ppush(&blk->phicpys, &cpy);
    }
}

static void ir_findblkcpys(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;

    for(i=0; i<blk->insts.len; i++)
    {
        if(blk->insts.data[i].op != IR_OP_PHI)
            continue;
            
        ir_findphicpys(funcdef, &blk->insts.data[i]);
        list_ir_inst_remove(&blk->insts, i);
        i--;
    }
}

static void ir_replacephiedge(ir_block_t* blk, const char* oldlabel, const char* newlabel)
{
    int i, j;

    ir_inst_t *inst;
    ir_operand_t *operand;

    for(i=0, inst=blk->insts.data; i<blk->insts.len; i++, inst++)
    {
        // all phi instructions are at beginning of block,
        // so we can break rather than continue
        if(inst->op != IR_OP_PHI)
            break;

        for(j=1, operand=inst->variadic.data+1; j<inst->variadic.len; j+=2, operand+=2)
        {
            if(strcmp(operand->label, oldlabel))
                continue;
            free(operand->label);
            operand->label = strdup(newlabel);
        }
    }
}

static void ir_elimcriticaledges(ir_funcdef_t* funcdef)
{
    int i, b, e;
    ir_block_t *blk;

    ir_block_t *edge, *newblk;
    ir_inst_t inst, *pinst;
    uint64_t idx, phiblk;
    bool foundedge;

    do
    {
        foundedge = false;
        for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
        {
            if(blk->out.len <= 1)
                continue;
            
            for(e=0; e<blk->out.len; e++)
            {
                edge = blk->out.data[e];
                if(edge->in.len <= 1)
                    continue;

                phiblk = edge - funcdef->blocks.data;

                // critical edge!

                idx = ir_newblock(funcdef);
                blk = &funcdef->blocks.data[b];
                edge = &funcdef->blocks.data[phiblk];
                newblk = &funcdef->blocks.data[idx];

                ir_replacephiedge(edge, blk->name, newblk->name);

                pinst = &blk->insts.data[blk->insts.len-1];
                for(i=1; i<3; i++)
                {
                    if(strcmp(pinst->ternary[i].label, edge->name))
                        continue;
                    free(pinst->ternary[i].label);
                    pinst->ternary[i].label = strdup(newblk->name);
                }

                inst.op = IR_OP_JMP;
                inst.unary.type = IR_OPERAND_LABEL;
                inst.unary.label = strdup(edge->name);
                list_ir_inst_ppush(&newblk->insts, &inst);

                ir_flow();

                foundedge = true;
                break;
            }

            if(foundedge)
                break;
        }
    } while(foundedge);
}

static void ir_lowerfunc(ir_funcdef_t* funcdef)
{
    int i;

    ir_elimcriticaledges(funcdef);

    for(i=0; i<funcdef->blocks.len; i++)
        ir_findblkcpys(funcdef, &funcdef->blocks.data[i]);

    for(i=0; i<funcdef->blocks.len; i++)
        ir_sequentializecpys(funcdef, &funcdef->blocks.data[i]);
}

void ir_lower(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        ir_lowerfunc(&ir.defs.data[i]);
}