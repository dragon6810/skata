#include "ir.h"

#include <assert.h>
#include <stdio.h>

#include "ast.h"

void ir_regcpy(ir_reg_t* dst, ir_reg_t* src)
{
    dst->name = strdup(src->name);
    memcpy(&dst->life, &src->life, sizeof(map_u64_ir_regspan_t));
    dst->life.bins = malloc(sizeof(map_u64_ir_regspan_el_t) * dst->life.nbin);
    memcpy(dst->life.bins, src->life.bins, sizeof(map_u64_ir_regspan_el_t) * dst->life.nbin);\
    dst->hardreg = src->hardreg;
}

void ir_regfree(ir_reg_t* reg)
{
    free(reg->name);
    map_u64_ir_regspan_free(&reg->life);
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
    list_pir_block_free(&block->in);
    list_pir_block_free(&block->out);
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
    map_u64_ir_regspan_alloc(&reg.life);
    preg = map_str_ir_reg_set(&funcdef->regs, &reg.name, &reg);
    free(reg.name);
    
    return preg;
}

static void ir_newblock(ir_funcdef_t* funcdef)
{
    ir_block_t blk;
    char blkname[64];

    snprintf(blkname, 64, "%d", (int) funcdef->blocks.len);
    blk.name = strdup(blkname);
    list_ir_inst_init(&blk.insts, 0);
    list_pir_block_init(&blk.in, 0);
    list_pir_block_init(&blk.out, 0);
    list_ir_block_ppush(&funcdef->blocks, &blk);
}

static ir_reg_t* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr, ir_reg_t* outreg);

// if outreg is NULL, it will alloc a new register
static ir_reg_t* ir_gen_ternary(ir_funcdef_t* funcdef, expr_t* expr, ir_reg_t* outreg)
{
    int iblk;
    ir_reg_t *res;
    ir_inst_t inst, *pinst;
    ir_block_t *pblk;

    iblk = funcdef->blocks.len - 1;

    inst.op = IR_OP_BZ;
    inst.binary[0].type = IR_OPERAND_REG;
    inst.binary[0].reg = ir_gen_expr(funcdef, expr->operands[0], NULL);
    list_ir_inst_ppush(&funcdef->blocks.data[iblk].insts, &inst);

    inst.binary[0].reg = res = outreg ? outreg : ir_gen_alloctemp(funcdef);
    
    // if block
    ir_newblock(funcdef);
    ir_gen_expr(funcdef, expr->operands[1], res);

    // skip over else block
    inst.op = IR_OP_BR;
    inst.binary[0].type = IR_OPERAND_LABEL;
    inst.binary[0].ilabel = funcdef->blocks.len+1;
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    // else block
    ir_newblock(funcdef);
    ir_gen_expr(funcdef, expr->operands[2], res);

    pblk = &funcdef->blocks.data[iblk];
    pinst = &pblk->insts.data[pblk->insts.len-1];
    pinst->binary[1].type = IR_OPERAND_LABEL;
    pinst->binary[1].ilabel = funcdef->blocks.len-1;

    // rest of function continues from here
    ir_newblock(funcdef);

    return res;
}

// if outreg is NULL, it will alloc a new register
static ir_reg_t* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr, ir_reg_t* outreg)
{
    ir_reg_t *res;
    ir_inst_t inst;

    switch(expr->op)
    {
    case EXPROP_LIT:
        inst.op = IR_OP_MOVE;
        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].reg = res = outreg ? outreg : ir_gen_alloctemp(funcdef);

        inst.binary[1].type = IR_OPERAND_LIT;
        inst.binary[1].literal.type = IR_PRIM_I32;
        inst.binary[1].literal.i32 = atoi(expr->msg);
        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_VAR:
        inst.op = IR_OP_LOAD;

        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].reg = res = outreg ? outreg : ir_gen_alloctemp(funcdef);

        inst.binary[1].type = IR_OPERAND_VAR;
        inst.binary[1].var = map_str_ir_var_get(&funcdef->vars, &expr->msg);

        if(!inst.binary[1].var)
        {
            printf("undeclared identifier %s\n", expr->msg);
            exit(1);
        }

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_COND:
        return ir_gen_ternary(funcdef, expr, outreg);
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
        inst.binary[1].reg = ir_gen_expr(funcdef, expr->operands[1], NULL);

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return inst.binary[1].reg;
    default:
        assert(0 && "unsupported op by IR");
        break;
    }

    inst.trinary[1].type = IR_OPERAND_REG;
    inst.trinary[1].reg = ir_gen_expr(funcdef, expr->operands[0], NULL);

    inst.trinary[2].type = IR_OPERAND_REG;
    inst.trinary[2].reg = ir_gen_expr(funcdef, expr->operands[1], NULL);
    
    inst.trinary[0].type = IR_OPERAND_REG;
    inst.binary[0].reg = res = outreg ? outreg : ir_gen_alloctemp(funcdef);
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    return res;
}

static void ir_gen_return(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_inst_t inst;

    inst.op = IR_OP_RET;
    inst.unary.type = IR_OPERAND_REG;
    inst.unary.reg = ir_gen_expr(funcdef, expr, NULL);

    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt);

static void ir_gen_if(ir_funcdef_t* funcdef, ifstmnt_t* ifstmnt)
{
    int iblk;
    ir_block_t *pblk;
    ir_inst_t inst, *pinst;

    iblk = funcdef->blocks.len - 1;

    inst.op = IR_OP_BZ;
    inst.binary[0].type = IR_OPERAND_REG;
    inst.binary[0].reg = ir_gen_expr(funcdef, ifstmnt->expr, NULL);
    list_ir_inst_ppush(&funcdef->blocks.data[iblk].insts, &inst);
    
    // then
    ir_newblock(funcdef);
    ir_gen_statement(funcdef, ifstmnt->ifblk);

    // TODO: else

    // rest of function
    ir_newblock(funcdef);
    pblk = &funcdef->blocks.data[iblk];
    pinst = &pblk->insts.data[pblk->insts.len-1];
    pinst->binary[1].type = IR_OPERAND_LABEL;
    pinst->binary[1].ilabel = funcdef->blocks.len-1;
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt)
{
    switch(stmnt->form)
    {
    case STMNT_EXPR:
        ir_gen_expr(funcdef, stmnt->expr, NULL);
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

    ir_block_t blk;
    ir_funcdef_t funcdef;

    if(!globdecl->hasfuncdef)
        return;

    funcdef.name = strdup(globdecl->decl.ident);
    funcdef.ntempreg = 0;
    funcdef.varframe = 0;
    list_ir_block_init(&funcdef.blocks, 1);
    funcdef.blocks.data[0].name = strdup("entry");
    list_ir_inst_init(&funcdef.blocks.data[0].insts, 0);
    list_pir_block_init(&funcdef.blocks.data[0].in, 0);
    list_pir_block_init(&funcdef.blocks.data[0].out, 0);
    map_str_ir_reg_alloc(&funcdef.regs);
    map_str_ir_var_alloc(&funcdef.vars);

    for(i=0; i<globdecl->funcdef.decls.len; i++)
        ir_gen_decl(&funcdef, &globdecl->funcdef.decls.data[i]);
    for(i=0; i<globdecl->funcdef.stmnts.len; i++)
        ir_gen_statement(&funcdef, &globdecl->funcdef.stmnts.data[i]);

    blk.name = strdup("exit");
    list_ir_inst_init(&blk.insts, 0);
    list_pir_block_init(&blk.in, 0);
    list_pir_block_init(&blk.out, 0);
    list_ir_block_ppush(&funcdef.blocks, &blk);
    list_pir_block_init(&funcdef.blocks.data[0].in, 0);
    list_pir_block_init(&funcdef.blocks.data[0].out, 0);

    list_ir_funcdef_ppush(&ir.defs, &funcdef);
}

void ir_gen(void)
{
    int i;

    list_ir_funcdef_init(&ir.defs, 0);

    for(i=0; i<ast.len; i++)
        ir_gen_globaldecl(&ast.data[i]);

    ir_flow();
}

void ir_free(void)
{
    list_ir_funcdef_free(&ir.defs);
}
