#include "asmgen.h"

#include <assert.h>
#include <stdio.h>

#include "regalloc.h"

const char* armheader = 
".text\n"
".global _main\n\n";

// w0-w7 are used for function calling and need to be pushed often, so try to use others first.
const int narmreg = 12;
const int ncorruptable = 7;
const char* armregs[] =
{
    // corruptable
    "w9",
    "w10",
    "w11",
    "w12",
    "w13",
    "w14",
    "w15",

    // callee-saved
    "w19",
    "w20",
    "w21",
    "w22",
    "w23",
};

const int stackpad = 16;

static void armgen_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("%s", armregs[map_str_ir_reg_get(&funcdef->regs, &operand->regname)->hardreg]);
        break;
    case IR_OPERAND_LIT:
        printf("#%d", operand->literal.i32);
        break;
    case IR_OPERAND_VAR:
        printf("[sp, #%d]", operand->var->stackloc);
        break;
    case IR_OPERAND_LABEL:
        printf("_%s$%s", funcdef->name, funcdef->blocks.data[operand->ilabel].name);
        break;
    default:
        assert(0 && "unexpected operand");
        break;
    }
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
        printf("  MUL ");
        armgen_operand(funcdef, &inst->ternary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->ternary[2]);
        printf("\n");
        break;
    case IR_OP_RET:
        if(inst->unary.regname)
        {
            printf("  MOV w0, ");
            armgen_operand(funcdef, &inst->unary);
            printf("\n");
        }
        if(funcdef->varframe)
            printf("  ADD sp, sp, #%d\n", (int) funcdef->varframe);
        printf("  RET\n");
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
    default:
        printf("unimplemented ir inst for arm %d.\n", (int) inst->op);
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

static void armgen_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    if(funcdef->varframe % stackpad)
        funcdef->varframe += stackpad - (funcdef->varframe % stackpad);

    printf("_%s:\n", funcdef->name);

    if(funcdef->varframe)
        printf("  SUB sp, sp, #%d\n", (int) funcdef->varframe);

    for(i=0; i<funcdef->blocks.len; i++)
        armgen_block(funcdef, &funcdef->blocks.data[i]);

    printf("\n");
}

void asmgen_arm(void)
{
    int i;

    nreg = 13;
    reglifetime();
    regalloc();

    printf("%s", armheader);

    for(i=0; i<ir.defs.len; i++)
        armgen_funcdef(&ir.defs.data[i]);
}