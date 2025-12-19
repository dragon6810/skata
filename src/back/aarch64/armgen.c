#include "back/back.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include "back/regalloc.h"

const char* armheader = 
".text\n"
".global _main\n\n";

const int stackpad = 16;

set_pir_reg_t savedregs;

static void arm_genregnames(hardreg_t* reg)
{
    map_u64_str_alloc(&reg->names);

    reg->name[0] = 'x';
    map_u64_str_set(&reg->names, IR_PRIM_PTR, reg->name);
    map_u64_str_set(&reg->names, IR_PRIM_I64, reg->name);
    map_u64_str_set(&reg->names, IR_PRIM_U64, reg->name);

    reg->name[0] = 'w';
    map_u64_str_set(&reg->names, IR_PRIM_I8, reg->name);
    map_u64_str_set(&reg->names, IR_PRIM_U8, reg->name);
    map_u64_str_set(&reg->names, IR_PRIM_I16, reg->name);
    map_u64_str_set(&reg->names, IR_PRIM_U16, reg->name);
    map_u64_str_set(&reg->names, IR_PRIM_I32, reg->name);
    map_u64_str_set(&reg->names, IR_PRIM_U32, reg->name);

    reg->name[0] = 'r';
}

void back_init(void)
{
    hardreg_t reg;

    list_hardreg_init(&regpool, 0);

    reg.index = regpool.len;
    reg.flags = HARDREG_CALLER | HARDREG_PARAM | HARDREG_RETURN;
    reg.name = strdup("r0");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.index = regpool.len;
    reg.flags = HARDREG_CALLER | HARDREG_PARAM | HARDREG_RETURN;
    reg.name = strdup("r1");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.index = regpool.len;
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("r2");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("r3");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("r4");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("r5");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("r6");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_PARAM;
    reg.name = strdup("r7");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_INDIRECTADR;
    reg.name = strdup("r8");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("r9");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("r10");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("r11");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("r12");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("r13");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("r14");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER;
    reg.name = strdup("r15");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_SCRATCH;
    reg.name = strdup("r16");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = HARDREG_CALLER | HARDREG_SCRATCH;
    reg.name = strdup("r17");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r19");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r20");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r21");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r22");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r23");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r24");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r25");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r26");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r27");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);
    reg.flags = 0;
    reg.name = strdup("r28");
    arm_genregnames(&reg);
    list_hardreg_ppush(&regpool, &reg);

    regalloc_init();
}

static const char* armgen_loadinst(ir_primitive_e prim)
{
    switch(prim)
    {
    case IR_PRIM_I8:
        return "LDRSB";
    case IR_PRIM_U8:
        return "LDRB";
    case IR_PRIM_I16:
        return "LDRSH";
    case IR_PRIM_U16:
        return "LDRH";
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        return "LDR";
    case IR_PRIM_I64:
    case IR_PRIM_PTR:
    case IR_PRIM_U64:
        return "LDRD";
    default:
        return NULL;
    }
}

static const char* armgen_storeinst(ir_primitive_e prim)
{
    switch(prim)
    {
    case IR_PRIM_I8:
    case IR_PRIM_U8:
        return "STRB";
    case IR_PRIM_I16:
    case IR_PRIM_U16:
        return "STRH";
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        return "STR";
    case IR_PRIM_I64:
    case IR_PRIM_PTR:
    case IR_PRIM_U64:
        return "STRD";
    default:
        return NULL;
    }
}

static void armgen_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("%s", *map_u64_str_get(&map_str_ir_reg_get(&funcdef->regs, operand->reg.name)->hardreg->names, operand->reg.type));
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

static void armgen_emitcast(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    ir_reg_t *dst, *src;
    ir_primitive_e dstprim, srcprim;

    assert(inst->binary[0].type == IR_OPERAND_REG);
    assert(inst->binary[1].type == IR_OPERAND_REG);

    dstprim = inst->binary[0].reg.type;
    srcprim = inst->binary[1].reg.type;

    dst = map_str_ir_reg_get(&funcdef->regs, inst->binary[0].reg.name);
    src = map_str_ir_reg_get(&funcdef->regs, inst->binary[1].reg.name);

    switch(dstprim)
    {
    case IR_PRIM_I32:
        switch(srcprim)
        {
        case IR_PRIM_I16:
            printf("  SXTH %s, %s\n", 
                *map_u64_str_get(&dst->hardreg->names, IR_PRIM_I32), 
                *map_u64_str_get(&src->hardreg->names, IR_PRIM_I32));
            break;
        default:
            assert(0);
            break;
        }
        break;
    default:
        assert(0);
        break;
    }
}

static void armgen_emitparamcopy(ir_funcdef_t* funcdef, 
    bool stackparam, uint64_t* stackoffs, uint64_t* regparam, ir_operand_t* operand)
{
    int i;

    hardreg_t *scratch;
    ir_primitive_e prim;

    if(!stackparam)
    {
        switch(operand->type)
        {
        case IR_OPERAND_REG:
            prim = operand->reg.type;

            printf("  MOV %s, %s\n", *map_u64_str_get(&parampool.data[*regparam]->names, prim), 
                *map_u64_str_get(&map_str_ir_reg_get(&funcdef->regs, operand->reg.name)->hardreg->names, prim));
            break;
        case IR_OPERAND_VAR:
            prim = operand->var->type.prim;

            printf("  %s %s, [fp, #%d]\n", armgen_loadinst(prim), *map_u64_str_get(&parampool.data[*regparam]->names, prim), operand->var->stackloc);
            break;
        case IR_OPERAND_LIT:
            prim = operand->literal.type;

            printf("  MOV %s, #%d\n", *map_u64_str_get(&parampool.data[*regparam]->names, prim), operand->literal.i32);
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
        printf("  STR %s, [sp, #%d]\n", map_str_ir_reg_get(&funcdef->regs, operand->reg.name)->hardreg->name, (int) *stackoffs);
        break;
    case IR_OPERAND_VAR:
        printf("  %s %s, [fp, #%d]\n", armgen_loadinst(operand->var->type.prim), scratch->name, operand->var->stackloc);
        printf("  %s %s, [sp, #%d]\n", armgen_storeinst(operand->var->type.prim), scratch->name, (int) *stackoffs);
        break;
    case IR_OPERAND_LIT:
        printf("  MOV %s, #%"PRIi64"\n", scratch->name, operand->literal.u64);
        printf("  %s %s, [sp, #%d]\n", armgen_storeinst(operand->literal.type), scratch->name, (int) *stackoffs);
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

                    logical = map_str_ir_reg_get(&funcdef->regs, inst->variadic.data[j].reg.name);
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
static void armgen_populateliveset(set_pir_reg_t* set, ir_funcdef_t* funcdef, ir_block_t* blk, uint64_t inst)
{
    int i;

    ir_regspan_t *span;

    for(i=0, span=blk->spans.data; i<blk->spans.len; i++, span++)
    {
        if(span->span[0] >= inst || span->span[1] <= inst+1)
            continue;

        set_pir_reg_add(set, map_str_ir_reg_get(&funcdef->regs, span->reg));
    }
}

static void armgen_emitcall(ir_funcdef_t* funcdef, ir_block_t* blk, ir_inst_t* inst)
{
    int i;

    int nregparam;
    int stacktotal, stackcur, paramsize;
    set_pir_reg_t saved;
    ir_reg_t *reg;
    ir_primitive_e prim;

    set_pir_reg_alloc(&saved);
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
    reg = map_str_ir_reg_get(&funcdef->regs, inst->variadic.data[0].reg.name);
    if(reg->hardreg != retpool.data[0])
        printf("  MOV %s, %s\n", reg->hardreg->name, retpool.data[0]->name);

    // restore caller-saved
    for(i=0, stackcur=paramsize; i<saved.nbin; i++)
    {
        if(saved.bins[i].state != SET_EL_FULL)
            continue;
        
        prim = saved.bins[i].val->type;
        printf("  %s %s, [sp, #%d]\n", armgen_loadinst(prim), *map_u64_str_get(&saved.bins[i].val->hardreg->names, prim), stackcur);
        stackcur += 4;
    }

    if(stacktotal)
        printf("  ADD sp, sp, #%d\n", stacktotal);

    set_pir_reg_free(&saved);
}

static void armgen_emitload(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    ir_primitive_e prim;

    assert(inst->binary[0].type == IR_OPERAND_REG);
    assert(inst->binary[1].type == IR_OPERAND_VAR);

    prim = inst->binary[0].reg.type;

    printf("  %s ", armgen_loadinst(prim));
    armgen_operand(funcdef, &inst->binary[0]);
    printf(", ");
    armgen_operand(funcdef, &inst->binary[1]);
    printf("\n");
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
        armgen_emitload(funcdef, inst);
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
    case IR_OP_CAST:
        armgen_emitcast(funcdef, inst);
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

    for(i=savedoffs=0; i<savedregs.nbin; i++)
    {
        if(savedregs.bins[i].state != SET_EL_FULL)
            continue;

        printf("  %s %s, [fp, #%d]\n", 
            armgen_loadinst(savedregs.bins[i].val->type),
            *map_u64_str_get(&savedregs.bins[i].val->hardreg->names, savedregs.bins[i].val->type),
            savedoffs);

        ir_primbytesize(savedregs.bins[i].val->type);
    }

    framesize = (savedoffs + stackpad - 1) & ~(stackpad - 1);

    printf("  ADD sp, fp, #%d\n", framesize);
    
    printf("  LDP fp, lr, [fp]\n");
    printf("  RET\n");

    set_pir_reg_free(&savedregs);
}

static void armgen_populatesaveset(ir_funcdef_t* funcdef)
{
    int i;

    for(i=0; i<funcdef->regs.nbin; i++)
    {
        if(funcdef->regs.bins[i].state != MAP_EL_FULL)
            continue;

        set_pir_reg_add(&savedregs, &funcdef->regs.bins[i].val);
    }

    for(i=0; i<savedregs.nbin; i++)
    {
        if(savedregs.bins[i].state != SET_EL_FULL)
            continue;
        if(!set_phardreg_contains(&calleepool, savedregs.bins[i].val->hardreg))
            savedregs.bins[i].state = SET_EL_TOMB;
    }
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

    set_pir_reg_alloc(&savedregs);
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

void back_gen(void)
{
    int i;

    printf("%s", armheader);

    for(i=0; i<ir.defs.len; i++)
        armgen_funcdef(&ir.defs.data[i]);
}