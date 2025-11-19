#include "asmgen.h"

#include <stdio.h>

#include "regalloc.h"

const char* armheader = 
".text\n"
".global _main\n\n";

const char* armregs[] = { "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9", "w10", "w11", "w12", };

static void armgen_operand(ir_operand_t* operand)
{
    if(operand->constant)
        printf("#%d", operand->literal.i32);
    else
        printf("%s", armregs[operand->reg->hardreg]);
}

static void armgen_inst(ir_inst_t* inst)
{
    switch(inst->op)
    {
    case IR_OP_MOVE:
        printf("  MOV ");
        armgen_operand(&inst->binary[0]);
        printf(", ");
        armgen_operand(&inst->binary[1]);
        printf("\n");
        break;
    case IR_OP_ADD:
        printf("  ADD ");
        armgen_operand(&inst->trinary[0]);
        printf(", ");
        armgen_operand(&inst->trinary[1]);
        printf(", ");
        armgen_operand(&inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_SUB:
        printf("  SUB ");
        armgen_operand(&inst->trinary[0]);
        printf(", ");
        armgen_operand(&inst->trinary[1]);
        printf(", ");
        armgen_operand(&inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_MUL:
        printf("  MUL ");
        armgen_operand(&inst->trinary[0]);
        printf(", ");
        armgen_operand(&inst->trinary[1]);
        printf(", ");
        armgen_operand(&inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_RET:
        if(inst->unary.reg)
        {
            printf("  MOV %s, ", armregs[0]);
            armgen_operand(&inst->unary);
            printf("\n");
        }
        printf("  RET\n");
        break;
    default:
        break;
    }
}

static void armgen_funcdef(ir_funcdef_t* funcdef)
{
    int i;

    printf("_%s:\n", funcdef->name);

    for(i=0; i<funcdef->insts.len; i++)
        armgen_inst(&funcdef->insts.data[i]);

    printf("\n");
}

void asmgen_arm(void)
{
    int i;

    nreg = 13;
    regalloc();

    printf("%s", armheader);

    for(i=0; i<ir.defs.len; i++)
        armgen_funcdef(&ir.defs.data[i]);
}