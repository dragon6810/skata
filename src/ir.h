#ifndef _GEN_H
#define _GEN_H

#include <stdint.h>

#include "map.h"

typedef struct ir_regspan_s
{
    // the register is created at span[0]
    bool start;
    // span[0] is first instruction
    // span[1] is last instruction + 1
    // span[0] <= i < span[1]
    uint64_t span[2];
} ir_regspan_t;

MAP_DECL(uint64_t, ir_regspan_t, u64, ir_regspan)

typedef struct ir_reg_s
{
    // '%' prefix implicit
    char *name;

    map_u64_ir_regspan_t life;
    int hardreg;
} ir_reg_t;

typedef struct ir_var_s
{
    // '$' prefix implicit
    char *name;

    int stackloc;
} ir_var_t;

MAP_DECL(char*, ir_reg_t, str, ir_reg)
MAP_DECL(char*, ir_var_t, str, ir_var)

typedef enum
{
    IR_OP_MOVE=0, // dst, src
    IR_OP_ADD, // dst, a, b
    IR_OP_SUB, // dst, a, b
    IR_OP_MUL, // dst, a, b,
    IR_OP_RET, // [value]
    IR_OP_STORE, // dst, src
    IR_OP_LOAD, // dst, src
    IR_OP_BR, // label
    IR_OP_BZ, // value, label
    IR_OP_PHI, // dst, avalue, bvalue
    IR_OP_COUNT,
} ir_inst_e;

typedef enum ir_primitive_s
{
    IR_PRIM_I32=0,
    IR_PRIM_COUNT,
} ir_primitive_t;

typedef struct ir_constant_s
{
    ir_primitive_t type;
    int32_t i32;
} ir_constant_t;

typedef enum
{
    IR_OPERAND_REG=0,
    IR_OPERAND_LIT,
    IR_OPERAND_VAR,
    IR_OPERAND_LABEL,
} ir_operand_e;

typedef struct ir_operand_s
{
    ir_operand_e type;
    union
    {
        // TODO: reg and var become dangling
        // switch them to be names soon
        char *regname;
        ir_constant_t literal;
        ir_var_t *var;
        uint64_t ilabel;
    };
} ir_operand_t;

typedef struct ir_inst_s
{
    ir_inst_e op;
    union
    {
        ir_operand_t unary;
        ir_operand_t binary[2];
        ir_operand_t ternary[3];
    };
} ir_inst_t;

LIST_DECL(ir_inst_t, ir_inst)

typedef struct ir_block_s ir_block_t;

LIST_DECL(ir_block_t*, pir_block)

struct ir_block_s
{
    char* name;
    list_ir_inst_t insts;

    // control flow edges
    list_pir_block_t in, out;
    
    // temporary
    bool marked;
};

LIST_DECL(ir_block_t, ir_block)

typedef struct ir_funcdef_s
{
    // '@' prefix implicit
    char *name;

    uint64_t ntempreg;
    map_str_ir_reg_t regs;
    uint64_t varframe; // size of stack frame if it was purely vars
    map_str_ir_var_t vars;
    
    list_ir_block_t blocks;
} ir_funcdef_t;

LIST_DECL(ir_funcdef_t, ir_funcdef)

typedef struct ir_s
{
    list_ir_funcdef_t defs;
} ir_t;

extern ir_t ir;

void ir_gen(void);
void ir_flow(void);
void ir_lower(void);
bool ir_registerwritten(ir_inst_t* inst, const char* reg);
void ir_free(void);
void ir_dump(void);
void ir_dumpflow(void);

#endif