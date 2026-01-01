#ifndef _GEN_H
#define _GEN_H

#include <stdint.h>

#include "map.h"
#include "set.h"

#define IR_FPRIM_INTEGRAL 0x01
#define IR_FPRIM_SIGNED 0x02

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
    IR_PRIM_PTR=0,
    IR_PRIM_U1,

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

    // a 'fake' register replaced with a static expression in ASM.
    // for example, result of alloca will turn into [sp + #x]
    // regalloc ignores virtual registers, but their lifetimes are still calculated
    bool virtual;

    set_str_t interfere; // interference graph edges
    struct hardreg_s *hardreg;
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

MAP_DECL(char*, ir_reg_t, str, ir_reg)

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
    IR_OP_CMPNEQ, // dst, a, b
    IR_OP_BR, // value, truelabel, falselabel
    IR_OP_JMP, // label
    IR_OP_PHI, // (VARIADIC); dst, label1, reg1, label2, reg2, ... labeln, regn
    IR_OP_CALL, // (VARIADIC); dst, func, arg1, arg2, ... argn
    IR_OP_ZEXT, // dst, src
    IR_OP_SEXT, // dst, src
    IR_OP_TRUNC, // dst, src
    IR_OP_ALLOCA, // dst
    IR_OP_FIE, // frame index elimination (get ptr of stack var): dst, src
    IR_OP_COUNT,
} ir_inst_e;

typedef struct ir_constant_s
{
    ir_primitive_e type;
    union
    {
        uint64_t ptr;
        int8_t i8;
        uint8_t u8;
        int16_t i16;
        uint16_t u16;
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
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
        char *var; // name of reg that holds ptr to var
    };
} ir_location_t;

typedef enum
{
    IR_OPERAND_REG=0,
    IR_OPERAND_LIT,
    IR_OPERAND_LABEL,
    IR_OPERAND_FUNC,
} ir_operand_e;

typedef struct ir_operand_s
{
    ir_operand_e type;
    union
    {
        struct
        {
            char *name;
        } reg;
        ir_constant_t literal;
        char *label;
        char *func;
    };
} ir_operand_t;

typedef enum
{
    IR_COPY_REG=0,
    IR_COPY_VAR,
    IR_COPY_LIT,
} ir_copy_e;

typedef struct ir_copyoperand_s
{
    ir_copy_e type;
    union
    {
        char *reg;
        char *var;
        ir_constant_t lit;
    };
} ir_copyoperand_t;

typedef struct ir_copy_s
{
    char *dstreg;
    ir_operand_t src;
} ir_copy_t;

LIST_DECL(ir_operand_t, ir_operand)
LIST_DECL(ir_operand_t*, pir_operand)
LIST_DECL(ir_copy_t, ir_copy)

typedef struct ir_inst_s ir_inst_t;

struct ir_inst_s
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
    union
    {
        char* var; // what variable is this phi-node setting? can be NULL.
        bool hasval; // for return statements
        struct
        {
            ir_type_t type;
        } alloca;
    };

    ir_inst_t *next;   
};

LIST_DECL(ir_inst_t*, pir_inst)
SET_DECL(ir_inst_t*, pir_inst)
MAP_DECL(char*, ir_inst_t*, str, pir_inst)

typedef struct ir_block_s ir_block_t;

LIST_DECL(ir_block_t*, pir_block)
SET_DECL(ir_block_t*, pir_block)

struct ir_block_s
{
    char* name;
    ir_inst_t *insts;

    ir_inst_t *branch; // if there is a branch or jump inst, this will point to it
    map_str_pir_inst_t varphis; // phi-node instructions for variables

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

    ir_type_t rettype;
    list_ir_param_t params;

    uint64_t ntempreg;
    map_str_ir_reg_t regs;
    
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

uint32_t ir_primflags(ir_primitive_e prim);
// sets name to NULL
void ir_initblock(ir_block_t* block);
ir_primitive_e ir_regtype(ir_funcdef_t* funcdef, char* regname);
char* ir_gen_alloctemp(ir_funcdef_t *funcdef, ir_primitive_e type);
// frees the ptr itself
void ir_instfree(ir_inst_t* inst);
uint64_t ir_newblock(ir_funcdef_t* funcdef);
int ir_primbytesize(ir_primitive_e prim);
void ir_cpyoperand(ir_operand_t* dst, ir_operand_t* src);
bool ir_registerwritten(ir_inst_t* inst, char* reg);
// set shouldn't be initialized
void ir_definedregs(set_str_t* set, ir_inst_t* inst);
// set shouldn't be initialized
void ir_accessedregs(set_str_t* set, ir_inst_t* inst);
// list shouldn't be intialized
void ir_instoperands(list_pir_operand_t* list, ir_inst_t* inst);

void ir_free(void);
void ir_dump(void);
void ir_dumpflow(void);
void ir_dumpdomtree(void);

#endif