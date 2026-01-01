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
map_str_u64_t stackptrs;

static void arm_addreg(uint32_t flags, const char* name)
{
    hardreg_t reg;

    reg.flags = flags;
    reg.name = strdup(name);

    map_u64_str_alloc(&reg.names);
    reg.name[0] = 'x';
    map_u64_str_set(&reg.names, IR_PRIM_PTR, reg.name);
    map_u64_str_set(&reg.names, IR_PRIM_I64, reg.name);
    map_u64_str_set(&reg.names, IR_PRIM_U64, reg.name);
    reg.name[0] = 'w';
    map_u64_str_set(&reg.names, IR_PRIM_I8, reg.name);
    map_u64_str_set(&reg.names, IR_PRIM_U8, reg.name);
    map_u64_str_set(&reg.names, IR_PRIM_I16, reg.name);
    map_u64_str_set(&reg.names, IR_PRIM_U16, reg.name);
    map_u64_str_set(&reg.names, IR_PRIM_I32, reg.name);
    map_u64_str_set(&reg.names, IR_PRIM_U32, reg.name);
    reg.name[0] = 'r';

    list_hardreg_ppush(&regpool, &reg);
}

void back_init(void)
{
    list_hardreg_init(&regpool, 0);

    arm_addreg(HARDREG_CALLER | HARDREG_PARAM | HARDREG_RETURN, "r0");
    arm_addreg(HARDREG_CALLER | HARDREG_PARAM | HARDREG_RETURN, "r1");
    arm_addreg(HARDREG_CALLER | HARDREG_PARAM, "r2");
    arm_addreg(HARDREG_CALLER | HARDREG_PARAM, "r3");
    arm_addreg(HARDREG_CALLER | HARDREG_PARAM, "r4");
    arm_addreg(HARDREG_CALLER | HARDREG_PARAM, "r5");
    arm_addreg(HARDREG_CALLER | HARDREG_PARAM, "r6");
    arm_addreg(HARDREG_CALLER | HARDREG_PARAM, "r7");
    arm_addreg(HARDREG_CALLER | HARDREG_INDIRECTADR, "r8");
    arm_addreg(HARDREG_CALLER, "r9");
    arm_addreg(HARDREG_CALLER, "r10");
    arm_addreg(HARDREG_CALLER, "r11");
    arm_addreg(HARDREG_CALLER | HARDREG_SCRATCH, "r12");
    arm_addreg(HARDREG_CALLER, "r13");
    arm_addreg(HARDREG_CALLER, "r14");
    arm_addreg(HARDREG_CALLER, "r15");
    arm_addreg(HARDREG_CALLER, "r16");
    arm_addreg(HARDREG_CALLER, "r17");
    arm_addreg(0, "r18");
    arm_addreg(0, "r19");
    arm_addreg(0, "r20");
    arm_addreg(0, "r21");
    arm_addreg(0, "r22");
    arm_addreg(0, "r23");
    arm_addreg(0, "r24");
    arm_addreg(0, "r25");
    arm_addreg(0, "r26");
    arm_addreg(0, "r27");
    arm_addreg(0, "r28");
    
    regalloc_init();

    map_u64_u64_alloc(&typereductions);

    map_u64_u64_set(&typereductions, IR_PRIM_U1, IR_PRIM_U8);
    map_u64_u64_set(&typereductions, IR_PRIM_PTR, IR_PRIM_U64);
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
    case IR_PRIM_I64:
    case IR_PRIM_PTR:
    case IR_PRIM_U64:
        return "LDR";
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
    case IR_PRIM_I64:
    case IR_PRIM_U64:
        return "STR";
    default:
        return NULL;
    }
}

static void armgen_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("%s", 
            *map_u64_str_get(&map_str_ir_reg_get(&funcdef->regs, operand->reg.name)->hardreg->names, 
            ir_regtype(funcdef, operand->reg.name)));
        break;
    case IR_OPERAND_LIT:
        printf("#%d", operand->literal.i32);
        break;
    case IR_OPERAND_LABEL:
        printf("_%s$%s", funcdef->name, operand->label);
        break;
    default:
        assert(0 && "unexpected operand");
        break;
    }
}

static void armgen_emitext(ir_funcdef_t* funcdef, ir_inst_t* inst, bool issigned)
{
    ir_reg_t *dst, *src;

    const char *instname;

    assert(inst->binary[0].type == IR_OPERAND_REG);
    assert(inst->binary[1].type == IR_OPERAND_REG);

    dst = map_str_ir_reg_get(&funcdef->regs, inst->binary[0].reg.name);
    src = map_str_ir_reg_get(&funcdef->regs, inst->binary[1].reg.name);\
    
    switch(src->type)
    {
    case IR_PRIM_I8:
    case IR_PRIM_U8:
        if(issigned)
            instname = "SXTB";
        else
            instname = "UXTB";
        break;
    case IR_PRIM_I16:
    case IR_PRIM_U16:
        if(issigned)
            instname = "SXTH";
        else
            instname = "UXTH";
        break;
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        if(issigned)
            instname = "SXTW";
        else
            instname = "UXTW";
        break;
    default:
        assert(0);
    }

    printf("  %s %s, %s\n", 
        instname,
        *map_u64_str_get(&dst->hardreg->names, dst->type),
        *map_u64_str_get(&src->hardreg->names, src->type));
}

static void armgen_emittrunc(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    ir_reg_t *dst, *src;

    const char *instname;

    assert(inst->binary[0].type == IR_OPERAND_REG);
    assert(inst->binary[1].type == IR_OPERAND_REG);

    dst = map_str_ir_reg_get(&funcdef->regs, inst->binary[0].reg.name);
    src = map_str_ir_reg_get(&funcdef->regs, inst->binary[1].reg.name);\
    
    switch(dst->type)
    {
    case IR_PRIM_I32:
    case IR_PRIM_U32:
        instname = "MOV";
        break;
    case IR_PRIM_I16:
    case IR_PRIM_U16:
        printf("  AND %s, %s, #0xFFFF\n", 
        *map_u64_str_get(&dst->hardreg->names, dst->type),
        *map_u64_str_get(&src->hardreg->names, src->type));
        return;
    case IR_PRIM_I8:
    case IR_PRIM_U8:
        printf("  AND %s, %s, #0xFF\n", 
        *map_u64_str_get(&dst->hardreg->names, dst->type),
        *map_u64_str_get(&src->hardreg->names, dst->type));
        return;
    default:
        assert(0);
    }

    printf("  %s %s, %s\n", 
        instname,
        *map_u64_str_get(&dst->hardreg->names, dst->type),
        *map_u64_str_get(&src->hardreg->names, dst->type));
}

static void armgen_emitparamcopy(ir_funcdef_t* funcdef, 
    bool stackparam, uint64_t* stackoffs, uint64_t* regparam, ir_operand_t* operand)
{
    int i;

    ir_reg_t *reg;
    hardreg_t *scratch;
    ir_primitive_e prim;

    if(!stackparam)
    {
        switch(operand->type)
        {
        case IR_OPERAND_REG:
            prim = ir_regtype(funcdef, operand->reg.name);

            printf("  MOV %s, %s\n", *map_u64_str_get(&parampool.data[*regparam]->names, prim), 
                *map_u64_str_get(&map_str_ir_reg_get(&funcdef->regs, operand->reg.name)->hardreg->names, prim));
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
        reg = map_str_ir_reg_get(&funcdef->regs, operand->reg.name);
        printf("  %s %s, [sp, #%d]\n",
            armgen_storeinst(reg->type),
            *map_u64_str_get(&reg->hardreg->names, reg->type), (int) *stackoffs);
        break;
    case IR_OPERAND_LIT:
        printf("  MOV %s, #%"PRIi64"\n", scratch->name, operand->literal.u64);
        printf("  %s %s, [sp, #%d]\n", armgen_storeinst(operand->literal.type),
            *map_u64_str_get(&scratch->names, operand->literal.type), (int) *stackoffs);
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

static void armgen_emitcall(ir_funcdef_t* funcdef, ir_block_t* blk, int iinst, ir_inst_t* inst)
{
    int i;

    int nregparam;
    int stacktotal, stackcur, paramsize;
    set_pir_reg_t saved;
    ir_reg_t *reg;
    ir_primitive_e prim;

    set_pir_reg_alloc(&saved);
    armgen_populateliveset(&saved, funcdef, blk, iinst);

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
        
        printf("  %s %s, [sp, #%d]\n", 
            armgen_storeinst(saved.bins[i].val->type),
            *map_u64_str_get(&saved.bins[i].val->hardreg->names, saved.bins[i].val->type), 
            stackcur);
        stackcur += 4;
    }

    armgen_emitparams(funcdef, inst);

    printf("  BL _%s\n", inst->variadic.data[1].func);

    assert(inst->variadic.data[0].type == IR_OPERAND_REG);
    reg = map_str_ir_reg_get(&funcdef->regs, inst->variadic.data[0].reg.name);
    if(reg->hardreg != retpool.data[0])
        printf("  MOV %s, %s\n", 
            *map_u64_str_get(&reg->hardreg->names, inst->variadic.data[0].type), 
            *map_u64_str_get(&retpool.data[0]->names, inst->variadic.data[0].type));

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
    uint64_t *frameloc;
    ir_primitive_e prim;

    assert(inst->op == IR_OP_LOAD);

    prim = ir_regtype(funcdef, inst->binary[0].reg.name);

    printf("  %s ", armgen_loadinst(prim));

    armgen_operand(funcdef, &inst->binary[0]);
    printf(", ");
    
    frameloc = NULL;
    if(inst->binary[1].type == IR_OPERAND_REG)
        frameloc = map_str_u64_get(&stackptrs, inst->binary[1].reg.name);
    
    if(frameloc)
        printf("[sp, #%"PRIu64"]", *frameloc);
    else
        armgen_operand(funcdef, &inst->binary[1]);

    printf("\n");
}

static void armgen_emitstore(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    uint64_t *frameloc;
    ir_primitive_e prim;
    hardreg_t *src;

    assert(inst->op == IR_OP_STORE);

    if(inst->binary[1].type == IR_OPERAND_REG)
    {
        prim = ir_regtype(funcdef, inst->binary[1].reg.name);
        src = map_str_ir_reg_get(&funcdef->regs, inst->binary[1].reg.name)->hardreg;
    }
    else if(inst->binary[1].type == IR_OPERAND_LIT)
    {
        prim = inst->binary[1].literal.type;
        src = scratchlist.data[0];
        printf("  LDR %s, =0x%016"PRIx64"\n", *map_u64_str_get(&src->names, prim), inst->binary[1].literal.u64);
    }
    else
        assert(0);

    printf("  %s %s", armgen_storeinst(prim), *map_u64_str_get(&src->names, prim));
    printf(", ");
    
    frameloc = NULL;
    if(inst->binary[0].type == IR_OPERAND_REG)
        frameloc = map_str_u64_get(&stackptrs, inst->binary[0].reg.name);
    
    if(frameloc)
        printf("[sp, #%"PRIu64"]", *frameloc);
    else
        armgen_operand(funcdef, &inst->binary[0]);

    printf("\n");
}

static void armgen_emitmul(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    if(inst->ternary[2].type == IR_OPERAND_LIT)
    {
        printf("  MOV %s, ", *map_u64_str_get(&scratchlist.data[0]->names, inst->ternary[2].literal.type));
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");

        printf("  MUL ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", %s\n", *map_u64_str_get(&scratchlist.data[0]->names, inst->ternary[2].literal.type));
        
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

static void armgen_inst(ir_funcdef_t* funcdef, ir_block_t* blk, int iinst, ir_inst_t* inst)
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
        printf("  MOV %s, ", *map_u64_str_get(&retpool.data[0]->names, funcdef->rettype.prim));
        armgen_operand(funcdef, &inst->unary);
        printf("\n");
        printf("  B _%s$exit\n", funcdef->name);
        break;
    case IR_OP_STORE:
        armgen_emitstore(funcdef, inst);
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
    case IR_OP_CMPNEQ:
        /*
            CMP %a, %b
            CSET %dst, neq
        */
        printf("  CMP ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        printf("  CSET ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ne\n");
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
        armgen_emitcall(funcdef, blk, iinst, inst);
        break;
    case IR_OP_ZEXT:
        armgen_emitext(funcdef, inst, false);
        break;
    case IR_OP_SEXT:
        armgen_emitext(funcdef, inst, true);
        break;
    case IR_OP_TRUNC:
        armgen_emittrunc(funcdef, inst);
        break;
    case IR_OP_ALLOCA:
        break;
    case IR_OP_FIE:
        printf("  ADD ");
        armgen_operand(funcdef, &inst->binary[0]);
        printf(", sp, #%"PRIu64"\n", *map_str_u64_get(&stackptrs, inst->binary[1].reg.name));
        break;
    default:
        printf("unimplemented ir inst %d for arm.\n", (int) inst->op);
        assert(0);
        break;
    }
}

static void armgen_block(ir_funcdef_t* funcdef, ir_block_t* block)
{
    int i;
    ir_inst_t *inst;

    printf("_%s$%s:\n", funcdef->name, block->name);

    for(i=0, inst=block->insts; inst; i++, inst=inst->next)
        armgen_inst(funcdef, block, i, inst);
}

static uint64_t argmen_buildvarframe(ir_funcdef_t* funcdef, int offs)
{
    ir_inst_t *inst;

    ir_block_t *entry;
    uint64_t stacksize;

    stacksize = offs;
    entry = funcdef->blocks.data;
    for(inst=entry->insts; inst; inst=inst->next)
    {
        if(inst->op != IR_OP_ALLOCA)
            continue;
        
        assert(inst->alloca.type.type == IR_TYPE_PRIM);

        map_str_u64_set(&stackptrs, inst->unary.reg.name, stacksize);
        stacksize += ir_primbytesize(inst->alloca.type.prim);
    }

    return stacksize - offs;
}

static void armgen_epilouge(ir_funcdef_t* funcdef)
{
    int i;

    int framesize;
    int savedoffs;

    for(i=0, savedoffs=16; i<savedregs.nbin; i++)
    {
        if(savedregs.bins[i].state != SET_EL_FULL)
            continue;

        printf("  %s %s, [fp, #%d]\n", 
            armgen_loadinst(savedregs.bins[i].val->type),
            *map_u64_str_get(&savedregs.bins[i].val->hardreg->names, savedregs.bins[i].val->type),
            savedoffs);

        savedoffs += ir_primbytesize(savedregs.bins[i].val->type);
    }

    framesize = (savedoffs + stackpad - 1) & ~(stackpad - 1);

    printf("  LDP fp, lr, [sp]\n");
    printf("  ADD sp, sp, #%d\n", framesize);

    printf("  RET\n");

    set_pir_reg_free(&savedregs);
    map_str_u64_alloc(&stackptrs);
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

static void armgen_prolouge(ir_funcdef_t* funcdef)
{
    int i;
    ir_param_t *param;

    int framesize;
    int nregparam;
    int stackparamoffs;
    int savedoffs;
    ir_reg_t *reg;

    map_str_u64_alloc(&stackptrs);
    set_pir_reg_alloc(&savedregs);

    armgen_populatesaveset(funcdef);

    for(i=framesize=0; i<savedregs.nbin; i++)
    {
        if(savedregs.bins[i].state != SET_EL_FULL)
            continue;
        framesize += ir_primbytesize(savedregs.bins[i].val->type);
    }

    framesize = 16 + argmen_buildvarframe(funcdef, 16 + framesize) + framesize;
    framesize = (framesize + stackpad - 1) & ~(stackpad - 1); 

    printf("  SUB sp, sp, #%d\n", framesize);

    for(i=0, savedoffs=16; i<savedregs.nbin; i++)
    {
        if(savedregs.bins[i].state != SET_EL_FULL)
            continue;

        printf("  %s %s, [sp, #%d]\n", 
            armgen_storeinst(savedregs.bins[i].val->type),
            *map_u64_str_get(&savedregs.bins[i].val->hardreg->names, savedregs.bins[i].val->type), 
            savedoffs);
        savedoffs += ir_primbytesize(savedregs.bins[i].val->type);
    }

    printf("  STP fp, lr, [sp]\n");

    printf("  MOV fp, sp\n");
    
    for(i=nregparam=stackparamoffs=0, param=funcdef->params.data; i<funcdef->params.len; i++, param++)
    {
        assert(param->loc.type == IR_LOCATION_REG);

        reg = map_str_ir_reg_get(&funcdef->regs, param->loc.reg);
        if(nregparam < parampool.len)
        {

            if(strcmp(reg->hardreg->name, parampool.data[nregparam]->name))
                printf("  MOV %s, %s\n", 
                    *map_u64_str_get(&reg->hardreg->names, reg->type),
                    *map_u64_str_get(&parampool.data[nregparam]->names, reg->type));
            nregparam++;
            continue;
        }

        printf("  %s %s, [sp, #%d]\n", 
            armgen_loadinst(reg->type),
            *map_u64_str_get(&reg->hardreg->names, reg->type),
            framesize + stackparamoffs);
        stackparamoffs += 4;
    }
}

static void armgen_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    map_str_u64_alloc(&stackptrs);

    printf("_%s:\n", funcdef->name);

    armgen_prolouge(funcdef);

    for(i=0; i<funcdef->blocks.len; i++)
        armgen_block(funcdef, &funcdef->blocks.data[i]);

    armgen_epilouge(funcdef);

    printf("\n");

    map_str_u64_free(&stackptrs);
}

void back_gen(void)
{
    int i;

    printf("%s", armheader);

    for(i=0; i<ir.defs.len; i++)
        armgen_funcdef(&ir.defs.data[i]);
}