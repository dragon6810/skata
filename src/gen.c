#include "ir.h"

#include <assert.h>
#include <stdio.h>

#include "ast.h"

void ir_regcpy(ir_reg_t* dst, ir_reg_t* src)
{
    dst->name = strdup(src->name);
}

void ir_regfree(ir_reg_t* reg)
{
    free(reg->name);
}

void ir_freefuncdef(ir_funcdef_t* funcdef)
{
    free(funcdef->name);
    map_str_ir_reg_free(&funcdef->regs);
    list_ir_inst_free(&funcdef->insts);
}

MAP_DEF(char*, ir_reg_t, str, ir_reg, map_strhash, map_strcmp, map_strcpy, ir_regcpy, map_freestr, ir_regfree)
LIST_DEF(ir_inst)
LIST_DEF_FREE(ir_inst)
LIST_DEF(ir_funcdef)
LIST_DEF_FREE_DECONSTRUCT(ir_funcdef, ir_freefuncdef)

ir_t ir;

static ir_reg_t* ir_gen_alloctemp(ir_funcdef_t *funcdef)
{
    ir_reg_t reg, *preg;
    char name[16];

    snprintf(name, 16, "%llu", funcdef->ntempreg++);
    reg.name = strdup(name);
    preg = map_str_ir_reg_set(&funcdef->regs, &reg.name, &reg);
    free(reg.name);
    
    return preg;
}

static ir_reg_t* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_reg_t *res;
    ir_inst_t inst;

    res = ir_gen_alloctemp(funcdef);

    switch(expr->op)
    {
    case EXPROP_ATOM:
        inst.op = IR_OP_MOVE;
        inst.binary[0].constant = false;
        inst.binary[0].reg = res;
        inst.binary[1].constant = true;
        inst.binary[1].literal.type = IR_PRIM_I32;
        inst.binary[1].literal.i32 = atoi(expr->msg);
        list_ir_inst_ppush(&funcdef->insts, &inst);
        return res;
    case EXPROP_SUB:
        inst.op = IR_OP_SUB;
        break;
    case EXPROP_MULT:
        inst.op = IR_OP_MUL;
        break;
    default:
        assert(0 && "unsupported op by IR");
        break;
    }

    inst.binary[0].constant = false;
    inst.binary[0].reg = ir_gen_expr(funcdef, expr->operands[0]);
    inst.binary[1].constant = false;
    inst.binary[1].reg = ir_gen_expr(funcdef, expr->operands[1]);

    return res;
}

static void ir_gen_return(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_inst_t inst;

    inst.op = IR_OP_RET;
    inst.unary.constant = false;
    inst.unary.reg = ir_gen_expr(funcdef, expr);

    list_ir_inst_ppush(&funcdef->insts, &inst);
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt)
{
    switch(stmnt->form)
    {
    case STMNT_EXPR:
        ir_gen_expr(funcdef, stmnt->expr);
    case STMNT_RETURN:
        ir_gen_return(funcdef, stmnt->expr);
        break;
    default:
        break;
    }
}

static void ir_gen_globaldecl(globaldecl_t *globdecl)
{
    int i;

    ir_funcdef_t funcdef;

    if(!globdecl->hasfuncdef)
        return;

    funcdef.name = strdup(globdecl->decl.ident);
    funcdef.ntempreg = 0;
    list_ir_inst_init(&funcdef.insts, 0);
    map_str_ir_reg_alloc(&funcdef.regs);

    for(i=0; i<globdecl->funcdef.stmnts.len; i++)
        ir_gen_statement(&funcdef, &globdecl->funcdef.stmnts.data[i]);

    list_ir_funcdef_ppush(&ir.defs, &funcdef);
}

void ir_gen(void)
{
    int i;

    list_ir_funcdef_init(&ir.defs, 0);

    for(i=0; i<ast.len; i++)
        ir_gen_globaldecl(&ast.data[i]);
}

void ir_free(void)
{
    list_ir_funcdef_free(&ir.defs);
}