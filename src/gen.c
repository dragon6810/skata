#include "ir.h"

#include <assert.h>
#include <stdio.h>

#include "ast.h"

void ir_regcpy(ir_reg_t* dst, ir_reg_t* src)
{
    memcpy(dst, src, sizeof(ir_reg_t));
    
    dst->name = strdup(src->name);
    set_str_dup(&dst->interfere, &src->interfere);
    dst->hardreg = src->hardreg;
}

void ir_regfree(ir_reg_t* reg)
{
    free(reg->name);
    set_str_free(&reg->interfere);
}

void ir_varcpy(ir_var_t* dst, ir_var_t* src)
{
    memcpy(dst, src, sizeof(ir_var_t));
    dst->name = strdup(src->name);
}

void ir_cpyoperand(ir_operand_t* dst, ir_operand_t* src)
{
    memcpy(dst, src, sizeof(ir_operand_t));
    switch(dst->type)
    {
    case IR_OPERAND_REG:
        if(dst->reg.name)
            dst->reg.name = strdup(dst->reg.name);
        break;
    case IR_OPERAND_LABEL:
        if(dst->label)
            dst->label = strdup(dst->label);
        break;
    case IR_OPERAND_FUNC:
        if(dst->func)
            dst->func = strdup(dst->func);
        break;
    default:
        break;
    }
}

void ir_operandfree(ir_operand_t* operand)
{
    switch(operand->type)
    {
    case IR_OPERAND_REG:
        free(operand->reg.name);
        break;
    case IR_OPERAND_LABEL:
        free(operand->label);
        break;
    case IR_OPERAND_FUNC:
        free(operand->func);
        break;
    default:
        break;
    }
}

void ir_copyfree(ir_copy_t* copy)
{
    ir_operandfree(&copy->dst);
    ir_operandfree(&copy->src);
}

void ir_instfree(ir_inst_t* inst)
{
    int i;

    int noperand;

    if(inst->op == IR_OP_PHI && inst->var)
        free(inst->var);

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
    map_str_u64_free(&block->varphis);
    list_pir_block_free(&block->in);
    list_pir_block_free(&block->out);
    set_pir_block_free(&block->dom);
    list_pir_block_free(&block->domfrontier);
    list_pir_block_free(&block->domchildren);
    set_str_free(&block->regdefs);
    set_str_free(&block->reguses);
    set_str_free(&block->livein);
    set_str_free(&block->liveout);
    list_ir_regspan_free(&block->spans);
    list_ir_copy_free(&block->phicpys);
}

void ir_freeparam(ir_param_t* param)
{
    switch(param->loc.type)
    {
    case IR_LOCATION_REG:
        free(param->loc.reg);
        break;
    case IR_LOCATION_VAR:
        free(param->loc.var);
        break;
    default:
        break;
    }
}

void ir_freefuncdef(ir_funcdef_t* funcdef)
{
    free(funcdef->name);
    list_ir_param_free(&funcdef->params);
    map_str_ir_reg_free(&funcdef->regs);
    map_str_ir_var_free(&funcdef->vars);
    list_ir_block_free(&funcdef->blocks);
    list_pir_block_free(&funcdef->postorder);
}

static uint64_t ir_hashpblock(ir_block_t** blk)
{
    return (uint64_t) *blk;
}

MAP_DEF(char*, ir_reg_t, str, ir_reg, hash_str, map_strcmp, map_strcpy, ir_regcpy, map_freestr, ir_regfree)
MAP_DEF(char*, ir_var_t, str, ir_var, hash_str, map_strcmp, map_strcpy, ir_varcpy, map_freestr, ir_varfree)
LIST_DEF(ir_inst)
LIST_DEF_FREE_DECONSTRUCT(ir_inst, ir_instfree)
LIST_DEF(ir_block)
LIST_DEF_FREE_DECONSTRUCT(ir_block, ir_freeblock)
SET_DEF(ir_block_t*, pir_block, ir_hashpblock, NULL, NULL, NULL)
LIST_DEF(ir_funcdef)
LIST_DEF_FREE_DECONSTRUCT(ir_funcdef, ir_freefuncdef)
LIST_DEF(ir_operand)
LIST_DEF_FREE_DECONSTRUCT(ir_operand, ir_operandfree)
LIST_DEF(pir_operand)
LIST_DEF_FREE(pir_operand)
LIST_DEF(ir_param)
LIST_DEF_FREE_DECONSTRUCT(ir_param, ir_freeparam)
LIST_DEF(ir_copy)
LIST_DEF_FREE_DECONSTRUCT(ir_copy, ir_copyfree)

ir_t ir;

ir_primitive_e ir_type2prim(type_e type)
{
    switch(type)
    {
    case TYPE_I8:
        return IR_PRIM_I8;
    case TYPE_U8:
        return IR_PRIM_U8;
    case TYPE_I16:
        return IR_PRIM_I16;
    case TYPE_U16:
        return IR_PRIM_U16;
    case TYPE_I32:
        return IR_PRIM_I32;
    case TYPE_U32:
        return IR_PRIM_U32;
    case TYPE_I64:
        return IR_PRIM_I64;
    case TYPE_U64:
        return IR_PRIM_U64;
    case TYPE_FUNC:
        return IR_PRIM_PTR;
    default:
        printf("attempting to convert non-primitive type %d to an IR primitive!\n", (int) type);
        abort();
    }
}

void ir_initblock(ir_block_t* block)
{
    block->name = NULL;
    list_ir_inst_init(&block->insts, 0);
    map_str_u64_alloc(&block->varphis);
    list_pir_block_init(&block->in, 0);
    list_pir_block_init(&block->out, 0);
    set_pir_block_alloc(&block->dom);
    list_pir_block_init(&block->domfrontier, 0);
    list_pir_block_init(&block->domchildren, 0);
    set_str_alloc(&block->regdefs);
    set_str_alloc(&block->reguses);
    set_str_alloc(&block->livein);
    set_str_alloc(&block->liveout);
    list_ir_regspan_init(&block->spans, 0);
    list_ir_copy_init(&block->phicpys, 0);
    block->marked = false;
}

// YOU are responsible for the returned string
char* ir_gen_alloctemp(ir_funcdef_t *funcdef, ir_primitive_e type)
{
    ir_reg_t reg;
    char name[16];

    memset(&reg, 0, sizeof(reg));

    snprintf(name, 16, "%llu", (unsigned long long) funcdef->ntempreg++);
    reg.name = strdup(name);
    reg.type = type;
    set_str_alloc(&reg.interfere);
    map_str_ir_reg_set(&funcdef->regs, reg.name, reg);
    set_str_free(&reg.interfere);
    
    return reg.name;
}

uint64_t ir_newblock(ir_funcdef_t* funcdef)
{
    ir_block_t blk;
    char blkname[64];
    uint64_t idx;

    ir_initblock(&blk);

    snprintf(blkname, 64, "%d", (int) funcdef->blocks.len);
    blk.name = strdup(blkname);

    idx = funcdef->blocks.len;
    if(funcdef->blocks.len && !strcmp(funcdef->blocks.data[funcdef->blocks.len-1].name, "exit"))
    {
        idx = funcdef->blocks.len - 1;
        (*map_str_u64_get(&funcdef->blktbl, "exit"))++;
    }

    list_ir_block_insert(&funcdef->blocks, idx, blk);
    map_str_u64_set(&funcdef->blktbl, blk.name, idx);
    return idx;
}

// YOU are responsible for the returned string, unless you gave outreg
static char* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg);

ir_primitive_e ir_regtype(ir_funcdef_t* funcdef, char* regname)
{
    ir_reg_t *reg;

    reg = map_str_ir_reg_get(&funcdef->regs, regname);
    assert(reg);

    return reg->type;
}

// if outreg is NULL, it will alloc a new register
// YOU are responsible for the returned string, unless you gave outreg
static char* ir_gen_ternary(ir_funcdef_t* funcdef, expr_t* expr, char* outreg)
{
    int condblk, ifblk, elseblk;
    char *res;
    ir_inst_t inst, *pinst;
    ir_block_t *pblk;
    char *areg, *breg;

    condblk = funcdef->blocks.len - 1;

    // cmp
    inst.op = IR_OP_CMPEQ;
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].reg.name = ir_gen_alloctemp(funcdef, ir_type2prim(expr->operands[0]->type.type));
    inst.ternary[0].reg.type = ir_regtype(funcdef, inst.ternary[0].reg.name);
    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].reg.name = ir_gen_expr(funcdef, expr->operands[0], NULL);
    inst.ternary[1].reg.type = ir_regtype(funcdef, inst.ternary[1].reg.name);
    inst.ternary[2].type = IR_OPERAND_LIT;
    inst.ternary[2].literal.type = ir_type2prim(expr->operands[0]->type.type);
    inst.ternary[2].literal.u64 = 0;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);

    // branch
    inst.op = IR_OP_BR;
    inst.ternary[0].reg.name = strdup(inst.ternary[0].reg.name);
    inst.ternary[0].reg.type = ir_regtype(funcdef, inst.ternary[0].reg.name);
    inst.ternary[2].type = IR_OPERAND_LABEL;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);
    
    // if block
    ir_newblock(funcdef);
    funcdef->blocks.data[condblk].insts.data[funcdef->blocks.data[condblk].insts.len-1].ternary[2].label 
        = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
    areg = ir_gen_expr(funcdef, expr->operands[1], NULL);
    ifblk = funcdef->blocks.len - 1;

    // else block
    ir_newblock(funcdef);
    breg = ir_gen_expr(funcdef, expr->operands[2], NULL);
    elseblk = funcdef->blocks.len - 1;

    // patch branch instruction to branch to else block
    pblk = &funcdef->blocks.data[condblk];
    pinst = &pblk->insts.data[pblk->insts.len-1];
    pinst->binary[1].type = IR_OPERAND_LABEL;
    pinst->binary[1].label = strdup(funcdef->blocks.data[elseblk].name);

    ir_newblock(funcdef);

    // skip over else block during if block
    inst.op = IR_OP_JMP;
    inst.unary.type = IR_OPERAND_LABEL;
    inst.unary.label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
    list_ir_inst_ppush(&funcdef->blocks.data[ifblk].insts, &inst);


    // phi statement
    res = outreg ? outreg : ir_gen_alloctemp(funcdef, ir_type2prim(expr->operands[1]->type.type));
    inst.op = IR_OP_PHI;
    inst.var = NULL;
    list_ir_operand_init(&inst.variadic, 5);
    inst.variadic.data[0].type = IR_OPERAND_REG;
    inst.variadic.data[0].reg.name = strdup(res);
    inst.variadic.data[0].reg.type = ir_regtype(funcdef, res);
    inst.variadic.data[1].type = IR_OPERAND_LABEL;
    inst.variadic.data[1].label = strdup(funcdef->blocks.data[ifblk].name);
    inst.variadic.data[2].type = IR_OPERAND_REG;
    inst.variadic.data[2].reg.name = areg;
    inst.variadic.data[2].reg.type = ir_regtype(funcdef, areg);
    inst.variadic.data[3].type = IR_OPERAND_LABEL;
    inst.variadic.data[3].label = strdup(funcdef->blocks.data[elseblk].name);
    inst.variadic.data[4].type = IR_OPERAND_REG;
    inst.variadic.data[4].reg.name = breg;
    inst.variadic.data[4].reg.type = ir_regtype(funcdef, breg);
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    return res;
}

static char* ir_gen_funccall(ir_funcdef_t* funcdef, expr_t* expr, char* outreg)
{
    int i;

    char *res;
    ir_inst_t inst;
    ir_operand_t operand;

    inst.op = IR_OP_CALL;
    list_ir_operand_init(&inst.variadic, 2);

    inst.variadic.data[1].type = IR_OPERAND_FUNC;
    inst.variadic.data[1].func = strdup(expr->operand->msg);

    for(i=0; i<expr->variadic.len; i++)
    {
        operand.type = IR_OPERAND_REG;
        operand.reg.name = ir_gen_expr(funcdef, expr->variadic.data[i], NULL);
        operand.reg.type = ir_regtype(funcdef, operand.reg.name);
        list_ir_operand_push(&inst.variadic, operand);
    }

    if(outreg)
        res = outreg;
    else
        res = ir_gen_alloctemp(funcdef, ir_type2prim(expr->variadic.data[0]->type.type));
    
    inst.variadic.data[0].type = IR_OPERAND_REG;
    inst.variadic.data[0].reg.name = strdup(res);
    inst.variadic.data[0].reg.type = ir_regtype(funcdef, res);

    list_ir_inst_push(&funcdef->blocks.data[funcdef->blocks.len-1].insts, inst);

    return res;
}

// if outreg is NULL, it will alloc a new register
// YOU are responsible for the returned string, unless you gave outreg
char* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg)
{
    char *res;
    ir_inst_t inst;

    if(outreg)
        res = outreg;
    else
        res = ir_gen_alloctemp(funcdef, ir_type2prim(expr->type.type));

    switch(expr->op)
    {
    case EXPROP_LIT:
        inst.op = IR_OP_MOVE;
        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].reg.name = strdup(res);
        inst.binary[0].reg.type = ir_type2prim(expr->type.type);

        inst.binary[1].type = IR_OPERAND_LIT;
        inst.binary[1].literal.type = IR_PRIM_I32;
        inst.binary[1].literal.i32 = expr->i64;
        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_VAR:
        inst.op = IR_OP_LOAD;

        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].reg.name = strdup(res);
        inst.binary[0].reg.type = ir_regtype(funcdef, res);

        inst.binary[1].type = IR_OPERAND_VAR;
        inst.binary[1].var = map_str_ir_var_get(&funcdef->vars, expr->msg);

        if(!inst.binary[1].var)
        {
            printf("undeclared identifier %s\n", expr->msg);
            exit(1);
        }

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_COND:
        return ir_gen_ternary(funcdef, expr, res);
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
        inst.binary[0].var = map_str_ir_var_get(&funcdef->vars, expr->operands[0]->msg);

        inst.binary[1].type = IR_OPERAND_REG;
        inst.binary[1].reg.name = strdup(ir_gen_expr(funcdef, expr->operands[1], res));
        inst.binary[1].reg.type = ir_regtype(funcdef, inst.binary[1].reg.name);

        if(!inst.binary[0].var)
        {
            printf("undeclared identifier %s\n", expr->msg);
            exit(1);
        }

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    case EXPROP_CALL:
        return ir_gen_funccall(funcdef, expr, outreg);
    case EXPROP_CAST:
        inst.op = IR_OP_CAST;
        inst.binary[0].type = IR_OPERAND_REG;
        inst.binary[0].reg.name = strdup(res);
        inst.binary[0].reg.type = ir_regtype(funcdef, res);
        inst.binary[1].type = IR_OPERAND_REG;
        inst.binary[1].reg.name = strdup(ir_gen_expr(funcdef, expr->operand, NULL));
        inst.binary[1].reg.type = ir_regtype(funcdef, inst.binary[1].reg.name);

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
        return res;
    default:
        assert(0 && "unsupported op by IR");
        break;
    }

    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].reg.name = strdup(ir_gen_expr(funcdef, expr->operands[0], NULL));
    inst.ternary[1].reg.type = ir_regtype(funcdef, inst.ternary[1].reg.name);

    inst.ternary[2].type = IR_OPERAND_REG;
    inst.ternary[2].reg.name = strdup(ir_gen_expr(funcdef, expr->operands[1], NULL));
    inst.ternary[2].reg.type = ir_regtype(funcdef, inst.ternary[2].reg.name);
    
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].reg.name = strdup(res);
    inst.ternary[0].reg.type = ir_regtype(funcdef, res);
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    return res;
}

static void ir_gen_return(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_inst_t inst;

    inst.op = IR_OP_RET;
    inst.unary.type = IR_OPERAND_REG;
    inst.unary.reg.name = ir_gen_expr(funcdef, expr, NULL);
    inst.unary.reg.type = ir_regtype(funcdef, inst.unary.reg.name);
    inst.hasval = true;

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
    inst.ternary[0].reg.name = ir_gen_alloctemp(funcdef, ir_type2prim(ifstmnt->expr->type.type));
    inst.ternary[0].reg.type = ir_regtype(funcdef, inst.ternary[0].reg.name);
    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].reg.name = ir_gen_expr(funcdef, ifstmnt->expr, NULL);
    inst.ternary[1].reg.type = ir_regtype(funcdef, inst.ternary[1].reg.name);
    inst.ternary[2].type = IR_OPERAND_LIT;
    inst.ternary[2].literal.type = ir_type2prim(ifstmnt->expr->type.type);
    inst.ternary[2].literal.u64 = 0;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);

    // branch
    inst.op = IR_OP_BR;
    inst.ternary[0].type = IR_OPERAND_REG;
    inst.ternary[0].reg.name = strdup(inst.ternary[0].reg.name);
    inst.ternary[2].type = IR_OPERAND_LABEL;
    list_ir_inst_ppush(&funcdef->blocks.data[condblk].insts, &inst);
    
    // then
    ir_newblock(funcdef);

    pblk = &funcdef->blocks.data[condblk];
    pblk->insts.data[pblk->insts.len-1].ternary[2].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);

    ir_gen_statement(funcdef, ifstmnt->ifblk);

    // TODO: else

    // rest of function
    ir_newblock(funcdef);
    pblk = &funcdef->blocks.data[condblk];
    pinst = &pblk->insts.data[pblk->insts.len-1];
    pinst->binary[1].type = IR_OPERAND_LABEL;
    pinst->binary[1].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
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
    inst.ternary[0].reg.name = ir_gen_alloctemp(funcdef, ir_type2prim(whilestmnt->expr->type.type));
    inst.ternary[0].reg.type = ir_regtype(funcdef, inst.ternary[0].reg.name);
    inst.ternary[1].type = IR_OPERAND_REG;
    inst.ternary[1].reg.name = ir_gen_expr(funcdef, whilestmnt->expr, NULL);
    inst.ternary[1].reg.type = ir_regtype(funcdef, inst.ternary[1].reg.name);
    inst.ternary[2].type = IR_OPERAND_LIT;
    inst.ternary[2].literal.type = ir_type2prim(whilestmnt->expr->type.type);
    inst.ternary[2].literal.u64 = 0;
    list_ir_inst_ppush(&funcdef->blocks.data[condblock].insts, &inst);

    // if true, jump to loop body
    inst.op = IR_OP_BR;
    inst.ternary[0].reg.name = strdup(inst.ternary[0].reg.name);
    inst.ternary[2].type = IR_OPERAND_LABEL;
    iinst = funcdef->blocks.data[condblock].insts.len;
    list_ir_inst_ppush(&funcdef->blocks.data[condblock].insts, &inst);
    
    // body
    ir_newblock(funcdef);

    pblk = &funcdef->blocks.data[condblock];
    pblk->insts.data[pblk->insts.len-1].ternary[2].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);

    ir_gen_statement(funcdef, whilestmnt->body);

    // jump back to beginning
    inst.op = IR_OP_JMP;
    inst.unary.type = IR_OPERAND_LABEL;
    inst.unary.label = strdup(funcdef->blocks.data[condblock].name);
    list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);

    // rest of function
    ir_newblock(funcdef);
    pblk = &funcdef->blocks.data[condblock];
    pinst = &pblk->insts.data[iinst];
    pinst->ternary[1].type = IR_OPERAND_LABEL;
    pinst->ternary[1].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt)
{
    int i;

    switch(stmnt->form)
    {
    case STMNT_COMPOUND:
        for(i=0; i<stmnt->compound.stmnts.len; i++)
            ir_gen_statement(funcdef, &stmnt->compound.stmnts.data[i]);
        break;
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
    var.type.type = IR_TYPE_PRIM;
    var.type.prim = ir_type2prim(decl->type.type);
    funcdef->varframe += 4;

    map_str_ir_var_set(&funcdef->vars, decl->ident, var);

    free(var.name);
}

static void ir_gen_arglist(ir_funcdef_t* funcdef, list_decl_t* arglist)
{
    int i;

    ir_inst_t inst;
    ir_var_t var, *pvar;
    char *reg;
    ir_param_t param;

    for(i=0; i<arglist->len; i++)
    {
        reg = ir_gen_alloctemp(funcdef, ir_type2prim(arglist->data[i].type.type));

        param.name = strdup(arglist->data[i].ident);
        param.loc.type = IR_LOCATION_REG;
        param.loc.reg = reg;
        list_ir_param_push(&funcdef->params, param);

        var.name = arglist->data[i].ident;
        var.stackloc = funcdef->varframe;
        var.type.type = IR_TYPE_PRIM;
        var.type.prim = ir_type2prim(arglist->data[i].type.type);
        funcdef->varframe += 4;
        pvar = map_str_ir_var_set(&funcdef->vars, arglist->data[i].ident, var);

        inst.op = IR_OP_STORE;
        inst.binary[0].type = IR_OPERAND_VAR;
        inst.binary[0].var = pvar;
        inst.binary[1].type = IR_OPERAND_REG;
        inst.binary[1].reg.name = strdup(reg);
        inst.binary[1].reg.type = ir_type2prim(arglist->data[i].type.type);

        list_ir_inst_ppush(&funcdef->blocks.data[funcdef->blocks.len-1].insts, &inst);
    }
}

static void ir_gen_globaldecl(globaldecl_t *globdecl)
{
    int i;

    ir_block_t blk;
    ir_funcdef_t funcdef;

    if(!globdecl->hasfuncdef)
        return;

    funcdef.name = strdup(globdecl->decl.ident);
    list_ir_param_init(&funcdef.params, 0);
    funcdef.ntempreg = 0;
    funcdef.varframe = 0;
    list_ir_block_init(&funcdef.blocks, 1);
    map_str_u64_alloc(&funcdef.blktbl);
    map_str_ir_reg_alloc(&funcdef.regs);
    map_str_ir_var_alloc(&funcdef.vars);
    list_pir_block_init(&funcdef.postorder, 0);

    ir_initblock(&funcdef.blocks.data[0]);
    funcdef.blocks.data[0].name = strdup("entry");
    map_str_u64_set(&funcdef.blktbl, funcdef.blocks.data[0].name, 0);
    
    ir_gen_arglist(&funcdef, &globdecl->decl.args);

    for(i=0; i<globdecl->funcdef.decls.len; i++)
        ir_gen_decl(&funcdef, &globdecl->funcdef.decls.data[i]);
    for(i=0; i<globdecl->funcdef.stmnts.len; i++)
        ir_gen_statement(&funcdef, &globdecl->funcdef.stmnts.data[i]);

    ir_initblock(&blk);
    blk.name = strdup("exit");
    list_ir_block_ppush(&funcdef.blocks, &blk);
    map_str_u64_set(&funcdef.blktbl, blk.name, funcdef.blocks.len - 1);

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
