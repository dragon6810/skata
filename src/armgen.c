#include "asmgen.h"

#include <assert.h>
#include <stdio.h>

#include "regalloc.h"

const char* armheader = 
".text\n"
".global _main\n\n";

const char* armregs[] = { "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9", "w10", "w11", "w12", };
const int stackpad = 16;

static void armgen_operand(ir_funcdef_t* funcdef, ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        printf("%s", armregs[operand->reg->hardreg]);
        break;
    case IR_OPERAND_LIT:
        printf("#%d", operand->literal.i32);
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
        armgen_operand(funcdef, &inst->trinary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->trinary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_SUB:
        printf("  SUB ");
        armgen_operand(funcdef, &inst->trinary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->trinary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_MUL:
        printf("  MUL ");
        armgen_operand(funcdef, &inst->trinary[0]);
        printf(", ");
        armgen_operand(funcdef, &inst->trinary[1]);
        printf(", ");
        armgen_operand(funcdef, &inst->trinary[2]);
        printf("\n");
        break;
    case IR_OP_RET:
        if(inst->unary.reg && inst->unary.reg->hardreg)
        {
            printf("  MOV %s, ", armregs[0]);
            armgen_operand(funcdef, &inst->unary);
            printf("\n");
        }
        printf("  ADD sp, sp, #%d\n", (int) funcdef->varframe);
        printf("  RET\n");
        break;
    case IR_OP_STORE:
        printf("  STR %s, [sp, #%d]\n", armregs[inst->binary[1].reg->hardreg], inst->binary[0].var->stackloc);
        break;
    case IR_OP_LOAD:
        printf("  LDR %s, [sp, #%d]\n", armregs[inst->binary[0].reg->hardreg], inst->binary[1].var->stackloc);
        break;
    case IR_OP_BR:
        printf("  B ");
        armgen_operand(funcdef, &inst->unary);
        printf("\n");
        break;
    case IR_OP_BZ:
        printf("  CMP ");
        armgen_operand(funcdef, &inst->binary[0]);
        printf(", #0\n");
        printf("  BEQ ");
        armgen_operand(funcdef, &inst->binary[1]);
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

    printf("  SUB sp, sp, #%d\n", (int) funcdef->varframe);

    for(i=0; i<funcdef->blocks.len; i++)
        armgen_block(funcdef, &funcdef->blocks.data[i]);

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