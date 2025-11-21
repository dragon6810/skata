#include "ir.h"

#include <assert.h>
#include <stdio.h>

#include "ast.h"

void ir_regcpy(ir_reg_t* dst, ir_reg_t* src)
{
    dst->name = strdup(src->name);
    dst->life[0] = src->life[0];
    dst->life[1] = src->life[1];
    dst->hardreg = src->hardreg;
}

void ir_regfree(ir_reg_t* reg)
{
    free(reg->name);
}

void ir_varcpy(ir_var_t* dst, ir_var_t* src)
{
    dst->name = strdup(src->name);
    dst->stackloc = src->stackloc;
}

void ir_varfree(ir_var_t* var)
{
    free(var->name);
}

void ir_freeblock(ir_block_t* block)
{
    free(block->name);
    list_ir_inst_free(&block->insts);
}

void ir_freefuncdef(ir_funcdef_t* funcdef)
{
    free(funcdef->name);
    map_str_ir_reg_free(&funcdef->regs);
    map_str_ir_var_free(&funcdef->vars);
    list_ir_block_free(&funcdef->blocks);
}

MAP_DEF(char*, ir_reg_t, str, ir_reg, map_strhash, map_strcmp, map_strcpy, ir_regcpy, map_freestr, ir_regfree)
MAP_DEF(char*, ir_var_t, str, ir_var, map_strhash, map_strcmp, map_strcpy, ir_varcpy, map_freestr, ir_varfree)
LIST_DEF(ir_inst)
LIST_DEF_FREE(ir_inst)
LIST_DEF(ir_block)
LIST_DEF_FREE_DECONSTRUCT(ir_block, ir_freeblock)
LIST_DEF(ir_funcdef)
LIST_DEF_FREE_DECONSTRUCT(ir_funcdef, ir_freefuncdef)

ir_t ir;

static ir_reg_t* ir_gen_alloctemp(ir_funcdef_t *funcdef)
{
    ir_reg_t reg, *preg;
    char name[16];

    snprintf(name, 16, "%llu", (unsigned long long) funcdef->ntempreg++);
    reg.name = strdup(name);
    preg = map_str_ir_reg_set(&funcdef->regs, &reg.name, &reg);
    free(reg.name);
    
    return preg;
}

static ir_reg_t* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_reg_t *res;
    ir_inst_t inst;

    switch(expr->op)
    {
    case EXPROP_RVAL:
        inst.op = IR_OP_MOVE;
        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].reg = res = ir_gen_alloctemp(funcdef);

        inst.binary[1].type = IR_OPERAND_LIT;
        inst.binary[1].literal.type = IR_PRIM_I32;
        inst.binary[1].literal.i32 = atoi(expr->msg);
        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_LVAL:
        inst.op = IR_OP_LOAD;

        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].reg = res = ir_gen_alloctemp(funcdef);

        inst.binary[1].type = IR_OPERAND_VAR;
        inst.binary[1].var = map_str_ir_var_get(&funcdef->vars, &expr->msg);

        if(!inst.binary[1].var)
        {
            printf("undeclared identifier %s\n", expr->msg);
            exit(1);
        }

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_ADD:
        inst.op = IR_OP_ADD;
        break;
    case EXPROP_SUB:
        inst.op = IR_OP_SUB;
        break;
    case EXPROP_MULT:
        inst.op = IR_OP_MUL;
        break;
    case EXPROP_ASSIGN:
        inst.op = IR_OP_STORE;
        inst.binary[0].type = IR_OPERAND_VAR;
        inst.binary[0].var = map_str_ir_var_get(&funcdef->vars, &expr->operands[0]->msg);

        inst.binary[1].type = IR_OPERAND_REG;
        inst.binary[1].reg = ir_gen_expr(funcdef, expr->operands[1]);

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return inst.binary[1].reg;
    default:
        assert(0 && "unsupported op by IR");
        break;
    }

    inst.trinary[1].type = IR_OPERAND_REG;
    inst.trinary[1].reg = ir_gen_expr(funcdef, expr->operands[0]);

    inst.trinary[2].type = IR_OPERAND_REG;
    inst.trinary[2].reg = ir_gen_expr(funcdef, expr->operands[1]);
    
    inst.trinary[0].type = IR_OPERAND_REG;
    inst.trinary[0].reg = res = ir_gen_alloctemp(funcdef);
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    return res;
}

static void ir_gen_return(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_inst_t inst;

    inst.op = IR_OP_RET;
    inst.unary.type = IR_OPERAND_REG;
    inst.unary.reg = ir_gen_expr(funcdef, expr);

    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt);

static void ir_newblock(ir_funcdef_t* funcdef)
{
    ir_block_t blk;
    char blkname[64];

    snprintf(blkname, 64, "%d", (int) funcdef->blocks.len);
    blk.name = strdup(blkname);
    list_ir_inst_init(&blk.insts, 0);
    list_ir_block_ppush(&funcdef->blocks, &blk);
}

static void ir_gen_if(ir_funcdef_t* funcdef, ifstmnt_t* ifstmnt)
{
    ir_inst_t inst;

    inst.op = IR_OP_BZ;
    inst.binary[0].type = IR_OPERAND_REG;
    inst.binary[0].reg = ir_gen_expr(funcdef, ifstmnt->expr);
    inst.binary[1].type = IR_OPERAND_LABEL;
    inst.binary[1].ilabel = funcdef->blocks.len+1;
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
    
    ir_newblock(funcdef);

    ir_gen_statement(funcdef, ifstmnt->ifblk);

    ir_newblock(funcdef);
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt)
{
    switch(stmnt->form)
    {
    case STMNT_EXPR:
        ir_gen_expr(funcdef, stmnt->expr);
        break;
    case STMNT_RETURN:
        ir_gen_return(funcdef, stmnt->expr);
        break;
    case STMNT_IF:
        ir_gen_if(funcdef, &stmnt->ifstmnt);
    default:
        break;
    }
}

static void ir_gen_decl(ir_funcdef_t* funcdef, decl_t* decl)
{
    ir_var_t var;

    var.name = strdup(decl->ident);
    var.stackloc = funcdef->varframe;
    funcdef->varframe += 4;

    map_str_ir_var_set(&funcdef->vars, &decl->ident, &var);

    free(var.name);
}

static void ir_gen_globaldecl(globaldecl_t *globdecl)
{
    int i;

    ir_funcdef_t funcdef;

    if(!globdecl->hasfuncdef)
        return;

    funcdef.name = strdup(globdecl->decl.ident);
    funcdef.ntempreg = 0;
    funcdef.varframe = 0;
    list_ir_block_init(&funcdef.blocks, 1);
    funcdef.blocks.data[0].name = strdup("entry");
    list_ir_inst_init(&funcdef.blocks.data[0].insts, 0);
    map_str_ir_reg_alloc(&funcdef.regs);
    map_str_ir_var_alloc(&funcdef.vars);

    for(i=0; i<globdecl->funcdef.decls.len; i++)
        ir_gen_decl(&funcdef, &globdecl->funcdef.decls.data[i]);
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
