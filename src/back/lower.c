#include "middle/middle.h"

#include <stdio.h>

#include "middle/ir.h"

// copies ownership of operands
static void insertcpy(ir_block_t* blk, ir_copy_t* cpy)
{
    ir_inst_t *last, *inst;

    ir_inst_t *cpyinst;

    for(last=NULL, inst=blk->insts; inst&&inst!=blk->branch; last=inst, inst=inst->next);

    cpyinst = malloc(sizeof(ir_inst_t));
    cpyinst->op = IR_OP_MOVE;
    cpyinst->binary[0].type = IR_OPERAND_REG;
    cpyinst->binary[0].reg.name = strdup(cpy->dstreg);
    ir_cpyoperand(&cpyinst->binary[1], &cpy->src);

    cpyinst->next = inst;
    if(last)
        last->next = cpyinst;
    else
        blk->insts = cpyinst;
}

static bool regssamelocation(ir_funcdef_t* funcdef, char* a, char* b)
{
    ir_reg_t *areg, *breg;

    areg = map_str_ir_reg_get(&funcdef->regs, a);
    breg = map_str_ir_reg_get(&funcdef->regs, b);
    assert(areg);
    assert(breg);

    if(!areg->hardreg && !breg->hardreg)
        return a == b;

    return areg->hardreg == breg->hardreg;
}

// returns true if dst is not used as any other src
static bool iscpyacyclic(ir_funcdef_t* funcdef, ir_block_t* blk, uint64_t idx)
{
    int i;

    for(i=0; i<blk->phicpys.len; i++)
    {
        if(idx == i)
            continue;

        if(blk->phicpys.data[i].src.type != IR_OPERAND_REG)
            continue;

        if(regssamelocation(funcdef, blk->phicpys.data[i].src.reg.name, blk->phicpys.data[idx].dstreg))
            return false;
    }

    return true;
}

static void sequentializecpys(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    int i;

    // weed out acyclic copies
    for(i=0; i<blk->phicpys.len; i++)
    {
        if(!iscpyacyclic(funcdef, blk, i))
            continue;

        insertcpy(blk, &blk->phicpys.data[i]);
        list_ir_copy_remove(&blk->phicpys, i);
        i--;
    }

    // TODO: resolve cyclic graphs
    assert(!blk->phicpys.len);
}

static void findphicpys(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    int i;
    ir_copy_t cpy;

    uint64_t *ilabel;
    ir_block_t *blk;

    assert(inst->op == IR_OP_PHI);
    assert(inst->variadic.len);
    assert(inst->variadic.data[0].type == IR_OPERAND_REG);

    for(i=2; i<inst->variadic.len; i+=2)
    {
        cpy.dstreg = strdup(inst->variadic.data[0].reg.name);
        ir_cpyoperand(&cpy.src, &inst->variadic.data[i]);

        ilabel = map_str_u64_get(&funcdef->blktbl, inst->variadic.data[i-1].label);
        assert(ilabel);
        blk = &funcdef->blocks.data[*ilabel];

        list_ir_copy_ppush(&blk->phicpys, &cpy);
    }
}

static void findblkcpys(ir_funcdef_t* funcdef, ir_block_t* blk)
{
    ir_inst_t *inst, *next, *last;

    for(last=NULL, inst=blk->insts; inst; last=inst, inst=next)
    {
        next = inst->next;

        if(inst->op != IR_OP_PHI)
            continue;
            
        findphicpys(funcdef, inst);
        if(last)
            last->next = inst->next;
        else
            blk->insts = inst->next;
        inst->next = NULL;
        ir_instfree(inst);
    }
}

static void replacephiedge(ir_block_t* blk, const char* oldlabel, const char* newlabel)
{
    int i;

    ir_inst_t *inst;
    ir_operand_t *operand;

    for(inst=blk->insts; inst; inst=inst->next)
    {
        if(inst->op != IR_OP_PHI)
            continue;

        for(i=1, operand=inst->variadic.data+1; i<inst->variadic.len; i+=2, operand+=2)
        {
            if(strcmp(operand->label, oldlabel))
                continue;
            free(operand->label);
            operand->label = strdup(newlabel);
        }
    }
}

static void elimcriticaledges(ir_funcdef_t* funcdef)
{
    int i, b, e;
    ir_block_t *blk;

    ir_block_t *edge, *newblk;
    ir_inst_t *inst, *pinst;
    uint64_t idx, phiblk;
    bool foundedge;

    do
    {
        foundedge = false;
        for(b=0, blk=funcdef->blocks.data; b<funcdef->blocks.len; b++, blk++)
        {
            if(blk->out.len <= 1)
                continue;

            assert(blk->branch);
            
            for(e=0; e<blk->out.len; e++)
            {
                edge = blk->out.data[e];
                if(edge->in.len <= 1)
                    continue;

                phiblk = edge - funcdef->blocks.data;

                // critical edge!

                idx = gen_newblock(funcdef);
                blk = &funcdef->blocks.data[b];
                edge = &funcdef->blocks.data[phiblk];
                newblk = &funcdef->blocks.data[idx];

                replacephiedge(edge, blk->name, newblk->name);

                pinst = blk->branch;
                for(i=1; i<3; i++)
                {
                    if(strcmp(pinst->ternary[i].label, edge->name))
                        continue;
                    free(pinst->ternary[i].label);
                    pinst->ternary[i].label = strdup(newblk->name);
                }

                inst = malloc(sizeof(ir_inst_t));
                inst->op = IR_OP_JMP;
                inst->unary.type = IR_OPERAND_LABEL;
                inst->unary.label = strdup(edge->name);
                inst->next = NULL;
                newblk->insts = inst;

                flow();

                foundedge = true;
                break;
            }

            if(foundedge)
                break;
        }
    } while(foundedge);
}

static void lowerfunc(ir_funcdef_t* funcdef)
{
    int i;

    elimcriticaledges(funcdef);

    for(i=0; i<funcdef->blocks.len; i++)
        findblkcpys(funcdef, &funcdef->blocks.data[i]);

    for(i=0; i<funcdef->blocks.len; i++)
        sequentializecpys(funcdef, &funcdef->blocks.data[i]);
}

void back_lower(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        lowerfunc(&ir.defs.data[i]);
}