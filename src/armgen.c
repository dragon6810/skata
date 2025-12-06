#include "asmgen.h"

#include <assert.h>
#include <stdio.h>

#include "regalloc.h"

const char* armheader = 
".text\n"
".global _main\n\n";

const int stackpad = 16;

set_phardreg_t savedregs;

void arm_specinit(void)
{
    hardreg_t reg;

    list_hardreg_init(&regpool, 0);

    reg.index = regpool.len;
    reg.flags = HARDREG_CALLER | HARDREG_PARAM | HARDREG_RETURN;
    reg.name = strdup("w0");
    list_hardreg_ppush(&regpool, &reg);
    reg.index = regpool.len;
    reg.flags = HARDREG_CALLER | HARDREG_PARAM | HARDREG_RETURN;
    reg.name = strdup("w1");
    list_hardreg_ppush(&regpool, &reg);
    reg.index = regpool.len;
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("w2");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("w3");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("w4");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("w5");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("w6");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("w7");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_INDIRECTADR;
    reg.name = strdup("w8");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("w9");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("w10");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("w11");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("w12");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("w13");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("w14");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("w15");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_SCRATCH;
    reg.name = strdup("w16");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_SCRATCH;
    reg.name = strdup("w17");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w19");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w20");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w21");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w22");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w23");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w24");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w25");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w26");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w27");
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("w28");
    list_hardreg_ppush(&regpool, &reg);
}

static void armgen_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("%s", map_str_ir_reg_get(&funcdef->regs, operand->regname)->hardreg->name);
        break;
    case IR_OPERAND_LIT:
        printf("#%d", operand->literal.i32);
        break;
    case IR_OPERAND_VAR:
        printf("[sp, #%d]", operand->var->stackloc);
        break;
    case IR_OPERAND_LABEL:
        printf("_%s$%s", funcdef->name, operand->label);
        break;
    default:
        assert(0 && "unexpected operand");
        break;
    }
}

static void armgen_emitparamcopy(ir_funcdef_t* funcdef, 
    bool stackparam, uint64_t* stackoffs, uint64_t* regparam, ir_operand_t* operand)
{
    int i;

    hardreg_t *scratch;

    if(!stackparam)
    {
        switch(operand->type)
        {
        case IR_OPERAND_REG:
            printf("  MOV %s, %s\n", parampool.data[*regparam]->name, 
                map_str_ir_reg_get(&funcdef->regs, operand->regname)->hardreg->name);
            break;
        case IR_OPERAND_VAR:
            printf("  LDR %s, [fp, #%d]\n", parampool.data[*regparam]->name, operand->var->stackloc);
            break;
        case IR_OPERAND_LIT:
            printf("  MOV %s, #%d\n", parampool.data[*regparam]->name, operand->literal.i32);
            break;
        default:
            assert(0);
        }

        (*regparam)++;
        return;
    }

    for(i=0; i<scratchpool.nbin; i++)
        if(scratchpool.bins[i].state == SET_EL_FULL)
            break;
    scratch = scratchpool.bins[i].val;

    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("  STR %s, [sp, #%d]\n", map_str_ir_reg_get(&funcdef->regs, operand->regname)->hardreg->name, (int) *stackoffs);
        break;
    case IR_OPERAND_VAR:
        printf("  LDR %s, [fp, #%d]\n", scratch->name, operand->var->stackloc);
        printf("  STR %s, [sp, #%d]\n", scratch->name, (int) *stackoffs);
        break;
    case IR_OPERAND_LIT:
        printf("  MOV %s, #%d\n", scratch->name, operand->literal.i32);
        printf("  STR %s, [sp, #%d]\n", scratch->name, (int) *stackoffs);
        break;
    default:
        assert(0);
    }

    *stackoffs += 4;
}

// sp should have already been moved
static void armgen_emitparams(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    int i, j;

    uint64_t nregarg, stackspot;
    set_u64_t stackargs;
    set_u64_t emitted;
    ir_reg_t *logical;
    bool change;

    set_u64_alloc(&emitted);
    set_u64_alloc(&stackargs);

    for(i=2, nregarg=0; i<inst->variadic.len; i++)
    {
        if(nregarg < parampool.len)
        {
            nregarg++;
            continue;
        }

        set_u64_add(&stackargs, i);
    }

    // acyclic parameters
    do
    {
        for(i=2, change=false, nregarg=stackspot=0; i<inst->variadic.len; i++)
        {
            if(set_u64_contains(&emitted, i))
            {
                if(set_u64_contains(&stackargs, i))
                    stackspot += 4;
                else
                    nregarg++;
                continue;
            }
            
            if(!set_u64_contains(&stackargs, i))
            {
                for(j=2; j<inst->variadic.len; j++)
                {
                    if(i == j || inst->variadic.data[j].type != IR_OPERAND_REG || set_u64_contains(&emitted, j))
                        continue;

                    logical = map_str_ir_reg_get(&funcdef->regs, inst->variadic.data[j].regname);
                    if(logical->hardreg == parampool.data[nregarg])
                        break;
                }
                // cycle!
                if(j < inst->variadic.len)
                {
                    nregarg++;
                    continue;
                }
            }

            armgen_emitparamcopy(funcdef, set_u64_contains(&stackargs, i), &stackspot, &nregarg, &inst->variadic.data[i]);
            set_u64_add(&emitted, i);
            change = true;
            break;
        }
    } while(change);

    assert(emitted.nfull == inst->variadic.len - 2);

    set_u64_free(&emitted);
    set_u64_free(&stackargs);
}

// populates with registers that "straddle" the instruction, meaning they are alive before and after it.
static void armgen_populateliveset(set_phardreg_t* set, ir_funcdef_t* funcdef, ir_block_t* blk, uint64_t inst)
{
    int i;

    ir_regspan_t *span;

    for(i=0, span=blk->spans.data; i<blk->spans.len; i++, span++)
    {
        if(span->span[0] >= inst || span->span[1] <= inst+1)
            continue;

        set_phardreg_add(set, map_str_ir_reg_get(&funcdef->regs, span->reg)->hardreg);
    }
}

static void armgen_emitcall(ir_funcdef_t* funcdef, ir_block_t* blk, ir_inst_t* inst)
{
    int i;

    int nregparam;
    int stacktotal, stackcur, paramsize;
    set_phardreg_t saved;
    ir_reg_t *reg;

    set_phardreg_alloc(&saved);
    armgen_populateliveset(&saved, funcdef, blk, inst - blk->insts.data);

    stacktotal = saved.nfull * 4;
    for(i=2, nregparam=paramsize=0; i<inst->variadic.len; i++)
    {
        if(nregparam < parampool.len)
        {
            nregparam++;
            continue;
        }

        stacktotal += 4;
        paramsize += 4;
    }

    stacktotal = (stacktotal + stackpad - 1) & ~(stackpad - 1); 
    if(stacktotal)
        printf("  SUB sp, sp, #%d\n", stacktotal);

    // save caller-saved
    for(i=0, stackcur=paramsize; i<saved.nbin; i++)
    {
        if(saved.bins[i].state != SET_EL_FULL)
            continue;
        
        printf("  STR %s, [sp, #%d]\n", saved.bins[i].val->name, stackcur);
        stackcur += 4;
    }

    armgen_emitparams(funcdef, inst);

    printf("  BL _%s\n", inst->variadic.data[1].func);

    assert(inst->variadic.data[0].type == IR_OPERAND_REG);
    reg = map_str_ir_reg_get(&funcdef->regs, inst->variadic.data[0].regname);
    if(reg->hardreg != retpool.data[0])
        printf("  MOV %s, %s\n", reg->hardreg->name, retpool.data[0]->name);

    // restore caller-saved
    for(i=0, stackcur=paramsize; i<saved.nbin; i++)
    {
        if(saved.bins[i].state != SET_EL_FULL)
            continue;
        
        printf("  LDR %s, [sp, #%d]\n", saved.bins[i].val->name, stackcur);
        stackcur += 4;
    }

    if(stacktotal)
        printf("  ADD sp, sp, #%d\n", stacktotal);

    set_phardreg_free(&saved);
}

static void armgen_emitmul(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    if(inst->ternary[2].type == IR_OPERAND_LIT)
    {
        printf("  MOV %s, ", scratchlist.data[0]->name);
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");

        printf("  MUL ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", %s\n", scratchlist.data[0]->name);
        
        return;
    }

    printf("  MUL ");
    armgen_operand(funcdef, &inst->ternary[0]);
    printf(", ");
    armgen_operand(funcdef, &inst->ternary[1]);
    printf(", ");
    armgen_operand(funcdef, &inst->ternary[2]);
    printf("\n");
}

static void armgen_inst(ir_funcdef_t* funcdef, ir_block_t* blk, ir_inst_t* inst)
{
    switch(inst->op)
    {
    case IR_OP_MOVE:
        printf("  MOV ");
        armgen_operand(funcdef, &inst->binary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_ADD:
        printf("  ADD ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        break;
    case IR_OP_SUB:
        printf("  SUB ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        break;
    case IR_OP_MUL:
        armgen_emitmul(funcdef, inst);
        break;
    case IR_OP_RET:
        printf("  MOV %s, ", retpool.data[0]->name);
        armgen_operand(funcdef, &inst->unary);
        printf("\n");
        printf("  B _%s$exit\n", funcdef->name);
        break;
    case IR_OP_STORE:
        printf("  STR ");
        armgen_operand(funcdef, &inst->binary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->binary[0]);
        printf("\n");
        break;
    case IR_OP_LOAD:
        printf("  LDR ");
        armgen_operand(funcdef, &inst->binary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_CMPEQ:
        /*
            CMP %a, %b
            CSET %dst, eq
        */
        printf("  CMP ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        printf("  CSET ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", eq\n");
        break;
    case IR_OP_BR:
        /*
            CBZ %reg, _false
            B _true
        */
        printf("  CBZ ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        printf("  B ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf("\n");
        break;
    case IR_OP_JMP:
        printf("  B ");
        armgen_operand(funcdef, &inst->unary);
        printf("\n");
        break;
    case IR_OP_CALL:
        armgen_emitcall(funcdef, blk, inst);
        break;
    default:
        printf("unimplemented ir inst %d for arm.\n", (int) inst->op);
        abort();
        break;
    }
}

static void armgen_block(ir_funcdef_t* funcdef, ir_block_t* block)
{
    int i;

    if(funcdef->varframe % stackpad)
        funcdef->varframe += stackpad - (funcdef->varframe % stackpad);

    printf("_%s$%s:\n", funcdef->name, block->name);

    for(i=0; i<block->insts.len; i++)
        armgen_inst(funcdef, block, &block->insts.data[i]);
}

static void armgen_funcfooter(ir_funcdef_t* funcdef)
{
    int i;

    int framesize;
    int savedoffs;

    framesize = funcdef->varframe + savedregs.nfull * 4 + 16;
    framesize = (framesize + stackpad - 1) & ~(stackpad - 1); 

    for(i=0, savedoffs=0; i<savedregs.nbin; i++)
    {
        if(savedregs.bins[i].state != SET_EL_FULL)
            continue;

        printf("  LDR %s, [fp, #%d]\n", savedregs.bins[i].val->name, savedoffs + 16);
        savedoffs += 4;
    }

    printf("  ADD sp, fp, #%d\n", framesize);
    
    printf("  LDP fp, lr, [fp]\n");
    printf("  RET\n");

    set_phardreg_free(&savedregs);
}

static void armgen_populatesaveset(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
    {
        if(funcdef->regs.bins[i].state != MAP_EL_FULL)
            continue;

        set_phardreg_add(&savedregs, funcdef->regs.bins[i].val.hardreg);
    }

    set_phardreg_intersection(&savedregs, &calleepool);
}

static void armgen_funcheader(ir_funcdef_t* funcdef)
{
    int i;
    ir_param_t *param;

    int framesize;
    int nregparam;
    int stackparamoffs;
    int savedoffs;
    ir_reg_t *reg;

    set_phardreg_alloc(&savedregs);
    armgen_populatesaveset(funcdef);

    framesize = funcdef->varframe + savedregs.nfull * 4 + 16;
    framesize = (framesize + stackpad - 1) & ~(stackpad - 1); 

    printf("  SUB sp, sp, #%d\n", framesize);

    for(i=0, savedoffs=0; i<savedregs.nbin; i++)
    {
        if(savedregs.bins[i].state != SET_EL_FULL)
            continue;

        printf("  STR %s, [sp, #%d]\n", savedregs.bins[i].val->name, savedoffs + 16);
        savedoffs += 4;
    }

    printf("  STP fp, lr, [sp]\n");

    printf("  MOV fp, sp\n");
    
    framesize = funcdef->varframe + 16;
    for(i=nregparam=stackparamoffs=0, param=funcdef->params.data; i<funcdef->params.len; i++, param++)
    {
        assert(param->loc.type == IR_LOCATION_REG);

        if(nregparam < parampool.len)
        {
            reg = map_str_ir_reg_get(&funcdef->regs, param->loc.reg);

            if(strcmp(reg->hardreg->name, parampool.data[nregparam]->name))
                printf("  MOV %s, %s\n", 
                    reg->hardreg->name,
                    parampool.data[nregparam]->name);
            nregparam++;
            continue;
        }

        printf("  LDR %s, [sp, #%d]\n", 
            map_str_ir_reg_get(&funcdef->regs, param->loc.reg)->hardreg->name,
            framesize + stackparamoffs);
        stackparamoffs += 4;
    }
}

static void armgen_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    if(funcdef->varframe % stackpad)
        funcdef->varframe += stackpad - (funcdef->varframe % stackpad);

    printf("_%s:\n", funcdef->name);

    armgen_funcheader(funcdef);

    for(i=0; i<funcdef->blocks.len; i++)
        armgen_block(funcdef, &funcdef->blocks.data[i]);

    armgen_funcfooter(funcdef);

    printf("\n");
}

void asmgen_arm(void)
{
    int i;

    printf("%s", armheader);

    for(i=0; i<ir.defs.len; i++)
        armgen_funcdef(&ir.defs.data[i]);
}