#include "asmgen.h"

#include <assert.h>
#include <stdio.h>

#include "regalloc.h"

const char* armheader = 
".text\n"
".global _main\n\n";

const int narmreg = 7;
static const char* armregs[] =
{
    "w4",
    "w5",
    "w6",
    "w7",
    "w8",
    "w9",
    "w10",
};
const int narmregparam = 4;
static const char* armregparams[] =
{
    "w0",
    "w1",
    "w2",
    "w3",
};
static const char* scratchreg = "w12";

const int stackpad = 16;

static void armgen_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("%s", armregs[map_str_ir_reg_get(&funcdef->regs, operand->regname)->hardreg]);
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

static void armgen_emitcall(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    int i;

    int nregparam;

    for(i=2, nregparam=0; i<inst->variadic.len; i++)
    {
        assert(nregparam < narmregparam);

        printf("  MOV %s, ", armregparams[nregparam]);
        armgen_operand(funcdef, &inst->variadic.data[i]);
        printf("\n");

        nregparam++;
    }

    printf("  BL _%s\n", inst->variadic.data[1].func);
    printf("  MOV ");
    armgen_operand(funcdef, &inst->variadic.data[0]);
    printf(", w0\n");
}

static void armgen_emitmul(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    if(inst->ternary[2].type == IR_OPERAND_LIT)
    {
        printf("  MOV %s, ", scratchreg);
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");

        printf("  MUL ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", %s\n", scratchreg);
        
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

static void armgen_inst(ir_funcdef_t* funcdef, ir_inst_t* inst)
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
        printf("  MOV w0, ");
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
        armgen_emitcall(funcdef, inst);
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
        armgen_inst(funcdef, &block->insts.data[i]);
}

static void armgen_funcfooter(ir_funcdef_t* funcdef)
{
    printf("  LDP fp, lr, [sp], #16\n");
    if(funcdef->varframe)
            printf("  ADD sp, sp, #%d\n", (int) funcdef->varframe);
    printf("  RET\n");
}

static void armgen_funcheader(ir_funcdef_t* funcdef)
{
    int i;
    ir_param_t *param;

    int nregparam;

    if(funcdef->varframe)
        printf("  SUB sp, sp, #%d\n", (int) funcdef->varframe);

    printf("  STP fp, lr, [sp, #-16]!\n");
    printf("  MOV fp, sp\n");
    
    for(i=nregparam=0, param=funcdef->params.data; i<funcdef->params.len; i++, param++)
    {
        assert(param->loc.type == IR_LOCATION_REG);
        assert(nregparam < narmregparam); // TODO: stack parameters

        printf("  MOV %s, %s\n", 
            armregs[map_str_ir_reg_get(&funcdef->regs, param->loc.reg)->hardreg],
            armregparams[nregparam]);

        nregparam++;
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