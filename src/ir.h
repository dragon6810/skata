#ifndef _GEN_H
#define _GEN_H

#include <stdint.h>

#include "map.h"
#include "set.h"
#include "type.h"

typedef struct ir_regspan_s
{
    char *reg;
    // span[0] is first instruction where it must be preserved
    // span[1] is last instruction where it must be preserved + 1 (last instruction used)
    uint64_t span[2];
} ir_regspan_t;

LIST_DECL(ir_regspan_t, ir_regspan)
MAP_DECL(char*, ir_regspan_t, str, ir_regspan)

typedef enum
{
    IR_PRIM_I8,
    IR_PRIM_U8,
    IR_PRIM_I16,
    IR_PRIM_U16,
    IR_PRIM_I32,
    IR_PRIM_U32,
    IR_PRIM_I64,
    IR_PRIM_U64,
    IR_PRIM_COUNT,
} ir_primitive_e;

typedef struct ir_reg_s
{
    // '%' prefix implicit
    char *name;
    ir_primitive_e type;

    set_str_t interfere; // interference graph edges
    struct hardreg_s *hardreg;
    int spill_offset; // stack offset from frame pointer if spilled
} ir_reg_t;

SET_DECL(ir_reg_t*, pir_reg)

typedef enum
{
    IR_TYPE_VOID=0,
    IR_TYPE_PRIM,
} ir_type_e;

typedef struct ir_type_s
{
    ir_type_e type;
    union
    {
        ir_primitive_e prim;
    };
} ir_type_t;

typedef struct ir_var_s
{
    // '$' prefix implicit
    char *name;

    ir_type_t type;
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
    IR_OP_CMPEQ, // dst, a, b
    IR_OP_BR, // value, truelabel, falselabel
    IR_OP_JMP, // label
    IR_OP_PHI, // (VARIADIC); dst, label1, reg1, label2, reg2, ... labeln, regn
    IR_OP_CALL, // (VARIADIC); dst, func, arg1, arg2, ... argn
    IR_OP_CAST, // dst, src
    IR_OP_COUNT,
} ir_inst_e;

typedef struct ir_constant_s
{
    ir_primitive_e type;
    union
    {
        int8_t i8;
        uint8_t u8;
        int16_t i16;
        uint16_t u16;
        int32_t i32;
        uint32_t u32;
        int32_t i64;
        uint32_t u64;
    };
} ir_constant_t;

typedef enum
{
    IR_LOCATION_REG=0,
    IR_LOCATION_VAR,
} ir_location_e;

typedef struct ir_location_s
{
    ir_location_e type;
    union
    {
        char *reg;
        char *var;
    };
} ir_location_t;

typedef enum
{
    IR_OPERAND_REG=0,
    IR_OPERAND_VAR,
    IR_OPERAND_LIT,
    IR_OPERAND_LABEL,
    IR_OPERAND_FUNC,
} ir_operand_e;

typedef struct ir_operand_s
{
    ir_operand_e type;
    union
    {
        // TODO: reg and var become dangling
        // switch them to be names soon
        struct
        {
            char *name;
            ir_primitive_e type;
        } reg;
        ir_constant_t literal;
        ir_var_t *var;
        char *label;
        char *func;
    };
} ir_operand_t;

typedef struct ir_copy_s
{
    ir_operand_t dst, src;
} ir_copy_t;

LIST_DECL(ir_operand_t, ir_operand)
LIST_DECL(ir_copy_t, ir_copy)

typedef struct ir_inst_s
{
    ir_inst_e op;
    union
    {
        ir_operand_t unary;
        ir_operand_t binary[2];
        ir_operand_t ternary[3];
        list_ir_operand_t variadic;
    };

    // opcode-specific studd
    char* var; // what variable is this phi-node setting? can be NULL.
} ir_inst_t;

LIST_DECL(ir_inst_t, ir_inst)

typedef struct ir_block_s ir_block_t;

LIST_DECL(ir_block_t*, pir_block)
SET_DECL(ir_block_t*, pir_block)

struct ir_block_s
{
    char* name;
    list_ir_inst_t insts;

    // index of phi-node instructions for variables
    map_str_u64_t varphis;

    // control flow edges
    list_pir_block_t in, out;

    // dominator stuff
    set_pir_block_t dom; // blocks that dominate this one
    list_pir_block_t domfrontier; // blocks that this one dominates
    ir_block_t *idom; // immediate dominator, parent in dom tree
    list_pir_block_t domchildren; // children in dom tree

    set_str_t regdefs;
    set_str_t reguses;
    set_str_t livein; // registers alive coming into the block
    set_str_t liveout; // registers alive going out of the block

    list_ir_regspan_t spans;

    list_ir_copy_t phicpys;
    
    // temporary
    bool marked;
};

LIST_DECL(ir_block_t, ir_block)

typedef struct ir_param_s
{
    char* name;
    ir_location_t loc;
} ir_param_t;

LIST_DECL(ir_param_t, ir_param)

typedef struct ir_funcdef_s
{
    // '@' prefix implicit
    char *name;

    list_ir_param_t params;

    uint64_t ntempreg;
    map_str_ir_reg_t regs;
    uint64_t varframe; // size of stack frame if it was purely vars
    uint64_t spillframe; // size of stack frame for spills
    map_str_ir_var_t vars;
    
    list_pir_block_t postorder;
    map_str_u64_t blktbl;
    list_ir_block_t blocks;
} ir_funcdef_t;

LIST_DECL(ir_funcdef_t, ir_funcdef)

typedef struct ir_s
{
    list_ir_funcdef_t defs;
} ir_t;

extern ir_t ir;

void ir_operandfree(ir_operand_t* operand);

void ir_gen(void);
void ir_flow(void);
void ir_ssa(void);
void ir_middleoptimize(void);
void ir_lower(void);
void ir_backoptimize(void);
// sets name to NULL
void ir_initblock(ir_block_t* block);
ir_primitive_e ir_type2prim(type_e type);
char* ir_gen_alloctemp(ir_funcdef_t *funcdef, ir_primitive_e type);
void ir_varfree(ir_var_t* var);
void ir_instfree(ir_inst_t* inst);
uint64_t ir_newblock(ir_funcdef_t* funcdef);
bool ir_operandeq(ir_funcdef_t* funcdef, ir_operand_t* a, ir_operand_t* b);
void ir_cpyoperand(ir_operand_t* dst, ir_operand_t* src);
bool ir_registerwritten(ir_inst_t* inst, char* reg);
// set shouldn't be initialized
void ir_definedregs(set_str_t* set, ir_inst_t* inst);
// set shouldn't be initialized
void ir_accessedregs(set_str_t* set, ir_inst_t* inst);

void ir_free(void);
void ir_dump(void);
void ir_dumpflow(void);
void ir_dumpdomtree(void);

#endif