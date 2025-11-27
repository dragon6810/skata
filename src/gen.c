#include "ir.h"

#include <assert.h>
#include <stdio.h>

#include "ast.h"

void ir_regcpy(ir_reg_t* dst, ir_reg_t* src)
{
    dst->name = strdup(src->name);
    memcpy(&dst->life, &src->life, sizeof(map_u64_ir_regspan_t));
    dst->life.bins = malloc(sizeof(map_u64_ir_regspan_el_t) * dst->life.nbin);
    memcpy(dst->life.bins, src->life.bins, sizeof(map_u64_ir_regspan_el_t) * dst->life.nbin);
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

void ir_operandfree(ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        free(operand->regname);
        break;
    default:
        break;
    }
}

void ir_instfree(ir_inst_t* inst)
{
    int i;

    int noperand;

    switch(inst->op)
    {
    case IR_OP_RET:
    case IR_OP_JMP:
        noperand = 1;
        break;
    case IR_OP_MOVE:
    case IR_OP_STORE:
    case IR_OP_LOAD:
        noperand = 2;
        break;
    case IR_OP_ADD:
    case IR_OP_SUB:
    case IR_OP_MUL:
    case IR_OP_CMPEQ:
    case IR_OP_BR:
        noperand = 3;
        break;
    // variadic
    case IR_OP_PHI:
        list_ir_operand_free(&inst->variadic);
        return;
    default:
        noperand = 0;
        break;
    }

    for(i=0; i<noperand; i++)
        ir_operandfree(&inst->ternary[i]);
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
    list_pir_block_free(&block->dom);
    list_pir_block_free(&block->domfrontier);
    list_pir_block_free(&block->domchildren);
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
LIST_DEF_FREE_DECONSTRUCT(ir_inst, ir_instfree)
LIST_DEF(ir_block)
LIST_DEF_FREE_DECONSTRUCT(ir_block, ir_freeblock)
LIST_DEF(ir_funcdef)
LIST_DEF_FREE_DECONSTRUCT(ir_funcdef, ir_freefuncdef)
LIST_DEF(ir_operand)
LIST_DEF_FREE_DECONSTRUCT(ir_operand, ir_operandfree)

ir_t ir;

// YOU are responsible for the returned string
static char* ir_gen_alloctemp(ir_funcdef_t *funcdef)
{
    ir_reg_t reg;
    char name[16];

    snprintf(name, 16, "%llu", (unsigned long long) funcdef->ntempreg++);
    reg.name = strdup(name);
    map_u64_ir_regspan_alloc(&reg.life);
    map_str_ir_reg_set(&funcdef->regs, &reg.name, &reg);
    
    return reg.name;
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
    list_pir_block_init(&blk.dom, 0);
    list_pir_block_init(&blk.domfrontier, 0);
    list_pir_block_init(&blk.domchildren, 0);
    list_ir_block_ppush(&funcdef->blocks, &blk);
}

// YOU are responsible for the returned string, unless you gave outreg
static char* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg);

// if outreg is NULL, it will alloc a new register
// YOU are responsible for the returned string, unless you gave outreg
static char* ir_gen_ternary(ir_funcdef_t* funcdef, expr_t* expr, char* outreg)
{
    int condblk, ifblk;
    char *res;
    ir_inst_t inst, *pinst;
    ir_block_t *pblk;
    char *areg, *breg;

    condblk = funcdef->blocks.len - 1;

    // cmp
    inst.op = IR_OP_CMPEQ;
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].regname = ir_gen_alloctemp(funcdef);
    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].regname = ir_gen_expr(funcdef, expr->operands[0], NULL);
    inst.ternary[2].type = IR_OPERAND_LIT;
    inst.ternary[2].literal.type = IR_PRIM_I32;
    inst.ternary[2].literal.i32 = 0;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);

    // branch
    inst.op = IR_OP_BR;
    inst.ternary[0].regname = strdup(inst.ternary[0].regname);
    inst.ternary[2].type = IR_OPERAND_LABEL;
    inst.ternary[2].ilabel = funcdef->blocks.len;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);
    
    // if block
    ir_newblock(funcdef);
    areg = ir_gen_expr(funcdef, expr->operands[1], NULL);
    ifblk = funcdef->blocks.len - 1;

    // else block
    ir_newblock(funcdef);
    breg = ir_gen_expr(funcdef, expr->operands[2], NULL);

    // skip over else block during if block
    inst.op = IR_OP_JMP;
    inst.unary.type = IR_OPERAND_LABEL;
    inst.unary.ilabel = funcdef->blocks.len;
    list_ir_inst_ppush(&funcdef->blocks.data[ifblk].insts, &inst);

    // patch branch instruction to branch to else block
    pblk = &funcdef->blocks.data[condblk];
    pinst = &pblk->insts.data[pblk->insts.len-1];
    pinst->binary[1].type = IR_OPERAND_LABEL;
    pinst->binary[1].ilabel = funcdef->blocks.len-1;

    // phi statement
    ir_newblock(funcdef);
    res = outreg ? outreg : ir_gen_alloctemp(funcdef);
    inst.op = IR_OP_PHI;
    list_ir_operand_init(&inst.variadic, 3);
    inst.variadic.data[0].type = IR_OPERAND_REG;
    inst.variadic.data[0].regname = strdup(res);
    inst.variadic.data[1].type = IR_OPERAND_REG;
    inst.variadic.data[1].regname = areg;
    inst.variadic.data[2].type = IR_OPERAND_REG;
    inst.variadic.data[2].regname = breg;
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    return res;
}

// if outreg is NULL, it will alloc a new register
// YOU are responsible for the returned string, unless you gave outreg
static char* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg)
{
    char *res;
    ir_inst_t inst;

    switch(expr->op)
    {
    case EXPROP_LIT:
        inst.op = IR_OP_MOVE;
        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].regname = strdup(res = outreg ? outreg : ir_gen_alloctemp(funcdef));

        inst.binary[1].type = IR_OPERAND_LIT;
        inst.binary[1].literal.type = IR_PRIM_I32;
        inst.binary[1].literal.i32 = atoi(expr->msg);
        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_VAR:
        inst.op = IR_OP_LOAD;

        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].regname = strdup(res = outreg ? outreg : ir_gen_alloctemp(funcdef));

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
        inst.binary[1].regname = strdup(res = ir_gen_expr(funcdef, expr->operands[1], outreg));

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    default:
        assert(0 && "unsupported op by IR");
        break;
    }

    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].regname = ir_gen_expr(funcdef, expr->operands[0], NULL);

    inst.ternary[2].type = IR_OPERAND_REG;
    inst.ternary[2].regname = ir_gen_expr(funcdef, expr->operands[1], NULL);
    
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].regname = strdup(res = outreg ? outreg : ir_gen_alloctemp(funcdef));
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    return res;
}

static void ir_gen_return(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_inst_t inst;

    inst.op = IR_OP_RET;
    inst.unary.type = IR_OPERAND_REG;
    inst.unary.regname = ir_gen_expr(funcdef, expr, NULL);

    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt);

static void ir_gen_if(ir_funcdef_t* funcdef, ifstmnt_t* ifstmnt)
{
    int condblk;
    ir_block_t *pblk;
    ir_inst_t inst, *pinst;

    condblk = funcdef->blocks.len - 1;

    // compare
    inst.op = IR_OP_CMPEQ;
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].regname = ir_gen_alloctemp(funcdef);
    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].regname = ir_gen_expr(funcdef, ifstmnt->expr, NULL);
    inst.ternary[2].type = IR_OPERAND_LIT;
    inst.ternary[2].literal.type = IR_PRIM_I32;
    inst.ternary[2].literal.i32 = 0;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);

    // branch
    inst.op = IR_OP_BR;
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].regname = strdup(inst.ternary[0].regname);
    inst.ternary[2].type = IR_OPERAND_LABEL;
    inst.ternary[2].ilabel = funcdef->blocks.len;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);
    
    // then
    ir_newblock(funcdef);
    ir_gen_statement(funcdef, ifstmnt->ifblk);

    // TODO: else

    // rest of function
    ir_newblock(funcdef);
    pblk = &funcdef->blocks.data[condblk];
    pinst = &pblk->insts.data[pblk->insts.len-1];
    pinst->binary[1].type = IR_OPERAND_LABEL;
    pinst->binary[1].ilabel = funcdef->blocks.len-1;
}

static void ir_gen_while(ir_funcdef_t* funcdef, whilestmnt_t* whilestmnt)
{
    int condblock, iinst;
    ir_block_t *pblk;
    ir_inst_t inst, *pinst;

    condblock = funcdef->blocks.len;
    ir_newblock(funcdef);

    // check for condition
    inst.op = IR_OP_CMPEQ;
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].regname = ir_gen_alloctemp(funcdef);
    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].regname = ir_gen_expr(funcdef, whilestmnt->expr, NULL);
    inst.ternary[2].type = IR_OPERAND_LIT;
    inst.ternary[2].literal.type = IR_PRIM_I32;
    inst.ternary[2].literal.i32 = 0;
    list_ir_inst_ppush(&funcdef->blocks.data[condblock].insts, &inst);

    // if true, jump to loop body
    inst.op = IR_OP_BR;
    inst.ternary[0].regname = strdup(inst.ternary[0].regname);
    inst.ternary[2].type = IR_OPERAND_LABEL;
    inst.ternary[2].ilabel = funcdef->blocks.len;
    iinst = funcdef->blocks.data[condblock].insts.len;
    list_ir_inst_ppush(&funcdef->blocks.data[condblock].insts, &inst);
    
    // body
    ir_newblock(funcdef);
    ir_gen_statement(funcdef, whilestmnt->body);

    // jump back to beginning
    inst.op = IR_OP_BR;
    inst.unary.type = IR_OPERAND_LABEL;
    inst.unary.ilabel = condblock;
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    // rest of function
    ir_newblock(funcdef);
    pblk = &funcdef->blocks.data[condblock];
    pinst = &pblk->insts.data[iinst];
    pinst->ternary[1].type = IR_OPERAND_LABEL;
    pinst->ternary[1].ilabel = funcdef->blocks.len-1;
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt)
{
    switch(stmnt->form)
    {
    case STMNT_EXPR:
        free(ir_gen_expr(funcdef, stmnt->expr, NULL));
        break;
    case STMNT_RETURN:
        ir_gen_return(funcdef, stmnt->expr);
        break;
    case STMNT_IF:
        ir_gen_if(funcdef, &stmnt->ifstmnt);
        break;
    case STMNT_WHILE:
        ir_gen_while(funcdef, &stmnt->whilestmnt);
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
    list_pir_block_init(&funcdef.blocks.data[0].dom, 0);
    list_pir_block_init(&funcdef.blocks.data[0].domfrontier, 0);
    list_pir_block_init(&funcdef.blocks.data[0].domchildren, 0);
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
    list_pir_block_init(&blk.dom, 0);
    list_pir_block_init(&blk.domfrontier, 0);
    list_pir_block_init(&blk.domchildren, 0);
    list_ir_block_ppush(&funcdef.blocks, &blk);
    list_pir_block_init(&funcdef.blocks.data[0].in, 0);
    list_pir_block_init(&funcdef.blocks.data[0].out, 0);
    list_pir_block_init(&funcdef.blocks.data[0].dom, 0);
    list_pir_block_init(&funcdef.blocks.data[0].domfrontier, 0);
    list_pir_block_init(&funcdef.blocks.data[0].domchildren, 0);

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
