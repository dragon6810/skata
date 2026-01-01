#include "middle/ir.h"

#include <assert.h>
#include <stdio.h>

#include "ast.h"

ir_t ir;

ir_inst_t *insttail;

static void gen_appendinst(ir_funcdef_t* funcdef, ir_inst_t* inst)
{
    if(inst->op == IR_OP_BR || inst->op == IR_OP_JMP)
        funcdef->blocks.data[funcdef->blocks.len-1].branch = inst;
        
    if(insttail)
    {
        insttail->next = inst;
        insttail = inst;
        return;
    }

    insttail = funcdef->blocks.data[funcdef->blocks.len-1].insts = inst;
}

static ir_inst_t* gen_allocinst(void)
{
    ir_inst_t *inst;

    inst = malloc(sizeof(ir_inst_t));
    inst->next = NULL;

    return inst;
}

void ir_initblock(ir_block_t* block)
{
    block->name = NULL;
    block->insts = block->branch = NULL;
    map_str_pir_inst_alloc(&block->varphis);
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

uint64_t gen_newblock(ir_funcdef_t* funcdef)
{
    ir_block_t blk;
    char blkname[64];
    uint64_t idx;

    insttail = NULL;

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

// if outreg is NULL, it will alloc a new register
// YOU are responsible for the returned string, unless you gave outreg
// TODO: make select instruction and do that instead of branching
static char* ir_gen_ternary(ir_funcdef_t* funcdef, expr_t* expr, char* outreg)
{
    ir_inst_t *inst, *brinst, *skipinst;
    int ifblk, elseblk;
    char *res;
    char *cmpres, *areg, *breg;

    // cmp
    inst = gen_allocinst();
    inst->op = IR_OP_CMPEQ;
    inst->ternary[0].type = IR_OPERAND_REG;
    inst->ternary[0].reg.name = cmpres = ir_gen_alloctemp(funcdef, IR_PRIM_U1);
    inst->ternary[1].type = IR_OPERAND_REG;
    inst->ternary[1].reg.name = ir_gen_expr(funcdef, expr->operands[0], NULL);
    inst->ternary[2].type = IR_OPERAND_LIT;
    inst->ternary[2].literal.type = type_toprim(expr->operands[0]->type.type);
    inst->ternary[2].literal.u64 = 0;
    gen_appendinst(funcdef, inst);

    // branch
    brinst = inst = gen_allocinst();
    inst->op = IR_OP_BR;
    inst->ternary[0].type = IR_OPERAND_REG;
    inst->ternary[0].reg.name = strdup(cmpres);
    inst->ternary[1].type = IR_OPERAND_LABEL;
    inst->ternary[2].type = IR_OPERAND_LABEL;
    gen_appendinst(funcdef, inst);
    
    // if block
    gen_newblock(funcdef);
    areg = ir_gen_expr(funcdef, expr->operands[1], NULL);
    ifblk = funcdef->blocks.len - 1;
    brinst->ternary[1].label = strdup(funcdef->blocks.data[ifblk].name);
    skipinst = inst = gen_allocinst();
    gen_appendinst(funcdef, inst);

    // else block
    gen_newblock(funcdef);
    breg = ir_gen_expr(funcdef, expr->operands[2], NULL);
    elseblk = funcdef->blocks.len - 1;
    brinst->ternary[2].label = strdup(funcdef->blocks.data[elseblk].name);

    gen_newblock(funcdef);

    // skip over else block during if block
    skipinst->op = IR_OP_JMP;
    skipinst->unary.type = IR_OPERAND_LABEL;
    skipinst->unary.label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);

    // phi statement
    res = outreg ? outreg : ir_gen_alloctemp(funcdef, type_toprim(expr->operands[1]->type.type));
    inst = gen_allocinst();
    inst->op = IR_OP_PHI;
    inst->var = NULL;
    list_ir_operand_init(&inst->variadic, 5);
    inst->variadic.data[0].type = IR_OPERAND_REG;
    inst->variadic.data[0].reg.name = strdup(res);
    inst->variadic.data[1].type = IR_OPERAND_LABEL;
    inst->variadic.data[1].label = strdup(funcdef->blocks.data[ifblk].name);
    inst->variadic.data[2].type = IR_OPERAND_REG;
    inst->variadic.data[2].reg.name = areg;
    inst->variadic.data[3].type = IR_OPERAND_LABEL;
    inst->variadic.data[3].label = strdup(funcdef->blocks.data[elseblk].name);
    inst->variadic.data[4].type = IR_OPERAND_REG;
    inst->variadic.data[4].reg.name = breg;
    gen_appendinst(funcdef, inst);

    return res;
}

static char* ir_gen_funccall(ir_funcdef_t* funcdef, expr_t* expr, char* outreg)
{
    int i;

    char *res;
    ir_inst_t *inst;
    ir_operand_t operand;

    inst = gen_allocinst();
    inst->op = IR_OP_CALL;
    list_ir_operand_init(&inst->variadic, 2);

    inst->variadic.data[1].type = IR_OPERAND_FUNC;
    inst->variadic.data[1].func = strdup(expr->operand->msg);

    for(i=0; i<expr->variadic.len; i++)
    {
        operand.type = IR_OPERAND_REG;
        operand.reg.name = ir_gen_expr(funcdef, expr->variadic.data[i], NULL);
        list_ir_operand_push(&inst->variadic, operand);
    }

    if(outreg)
        res = outreg;
    else
        res = ir_gen_alloctemp(funcdef, type_toprim(expr->type.type));
    
    inst->variadic.data[0].type = IR_OPERAND_REG;
    inst->variadic.data[0].reg.name = strdup(res);

    gen_appendinst(funcdef, inst);

    return res;
}

static char* ir_gen_cast(ir_funcdef_t* funcdef, expr_t* expr, char* outreg)
{
    ir_primitive_e dsttype, srctype;
    char *res;
    ir_inst_e opcode;
    ir_inst_t *inst;

    dsttype = type_toprim(expr->casttype.type);
    srctype = type_toprim(expr->operand->type.type);
    
    res = outreg ? outreg : ir_gen_alloctemp(funcdef, dsttype);

    opcode = IR_OP_MOVE;
    if(ir_primbytesize(dsttype) > ir_primbytesize(srctype))
    {
        if(ir_primflags(dsttype) & IR_FPRIM_SIGNED && ir_primflags(srctype) & IR_FPRIM_SIGNED)
            opcode = IR_OP_SEXT;
        else
            opcode = IR_OP_ZEXT;
    }
    else if(ir_primbytesize(dsttype) < ir_primbytesize(srctype))
        opcode = IR_OP_TRUNC;

    inst = gen_allocinst();
    inst->op = opcode;
    inst->binary[0].type = IR_OPERAND_REG;
    inst->binary[0].reg.name = strdup(res);
    inst->binary[1].type = IR_OPERAND_REG;
    inst->binary[1].reg.name = ir_gen_expr(funcdef, expr->operand, NULL);

    gen_appendinst(funcdef, inst);

    return res;
}

list_strpair_t varnames;

// return string belongs to the varnames
char* ir_gen_varname(const char* cname)
{
    int i;

    for(i=varnames.len-1; i>=0; i--)
        if(!strcmp(cname, varnames.data[i].a))
            return varnames.data[i].b;

    assert(0);
    return NULL;
}

char* ir_gen_lvaladr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg)
{
    ir_inst_t *inst;

    assert(expr->lval);
    assert(expr->op == EXPROP_VAR || expr->op == EXPROP_DEREF);

    if(expr->op == EXPROP_VAR)
    {
        if(!outreg)
            return strdup(ir_gen_varname(expr->msg));

        inst = gen_allocinst();
        inst->op = IR_OP_MOVE;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(outreg);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = strdup(ir_gen_varname(expr->msg));
        gen_appendinst(funcdef, inst);

        return outreg;
    }

    return ir_gen_expr(funcdef, expr->operand, outreg);
}

// if outreg is NULL, it will alloc a new register
// YOU are responsible for the returned string, unless you gave outreg
char* ir_gen_expr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg)
{
    char *res;
    ir_inst_t *inst;
    ir_inst_e opcode;

    if(outreg)
        res = outreg;
    else
        res = ir_gen_alloctemp(funcdef, type_toprim(expr->type.type));

    switch(expr->op)
    {
    case EXPROP_LIT:
        inst = gen_allocinst();
        inst->op = IR_OP_MOVE;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(res);
        inst->binary[1].type = IR_OPERAND_LIT;
        inst->binary[1].literal.type = IR_PRIM_I32;
        inst->binary[1].literal.i32 = expr->i64;
        gen_appendinst(funcdef, inst);
        return res;
    case EXPROP_VAR:
        inst = gen_allocinst();
        inst->op = IR_OP_LOAD;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(res);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = strdup(ir_gen_varname(expr->msg));
        gen_appendinst(funcdef, inst);
        return res;
    case EXPROP_COND:
        return ir_gen_ternary(funcdef, expr, res);
    case EXPROP_ADD:
        opcode = IR_OP_ADD;
        break;
    case EXPROP_SUB:
        opcode = IR_OP_SUB;
        break;
    case EXPROP_MULT:
        opcode = IR_OP_MUL;
        break;
    case EXPROP_NEQ:
        opcode = IR_OP_CMPNEQ;
        break;
    case EXPROP_ASSIGN:
        inst = gen_allocinst();
        inst->op = IR_OP_STORE;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = ir_gen_lvaladr(funcdef, expr->operands[0], NULL);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = strdup(ir_gen_expr(funcdef, expr->operands[1], res));
        gen_appendinst(funcdef, inst);
        return res;
    case EXPROP_REF:
        return ir_gen_lvaladr(funcdef, expr->operand, outreg);
    case EXPROP_CALL:
        return ir_gen_funccall(funcdef, expr, outreg);
    case EXPROP_LOGICNOT:
        inst = gen_allocinst();
        inst->op = IR_OP_CMPEQ;
        inst->ternary[0].type = IR_OPERAND_REG;
        inst->ternary[0].reg.name = strdup(res);
        inst->ternary[1].type = IR_OPERAND_REG;
        inst->ternary[1].reg.name = strdup(ir_gen_expr(funcdef, expr->operand, NULL));
        inst->ternary[2].type = IR_OPERAND_LIT;
        inst->ternary[2].literal.type = ir_regtype(funcdef, inst->ternary[1].reg.name);
        inst->ternary[2].literal.u64 = 0;
        gen_appendinst(funcdef, inst);
        return res;
    case EXPROP_CAST:
        return ir_gen_cast(funcdef, expr, outreg);
    default:
        assert(0 && "unsupported op by IR");
        break;
    }

    inst = gen_allocinst();
    inst->op = opcode;
    inst->ternary[0].type = IR_OPERAND_REG;
    inst->ternary[0].reg.name = strdup(res);
    inst->ternary[1].type = IR_OPERAND_REG;
    inst->ternary[1].reg.name = strdup(ir_gen_expr(funcdef, expr->operands[0], NULL));
    inst->ternary[2].type = IR_OPERAND_REG;
    inst->ternary[2].reg.name = strdup(ir_gen_expr(funcdef, expr->operands[1], NULL));
    gen_appendinst(funcdef, inst);
    return res;
}

static void ir_gen_return(ir_funcdef_t *funcdef, expr_t *expr)
{
    ir_inst_t *inst;

    inst = gen_allocinst();
    inst->op = IR_OP_RET;
    inst->hasval = false;
    if(expr)
    {
        inst->unary.type = IR_OPERAND_REG;
        inst->unary.reg.name = ir_gen_expr(funcdef, expr, NULL);
        inst->hasval = true;
    }

    gen_appendinst(funcdef, inst);
}

static void ir_gen_statement(ir_funcdef_t *funcdef, stmnt_t *stmnt);

static void ir_gen_if(ir_funcdef_t* funcdef, ifstmnt_t* ifstmnt)
{
    ir_inst_t *inst, *brinst;

    // branch
    brinst = inst = gen_allocinst();
    inst->op = IR_OP_BR;
    inst->ternary[0].type = IR_OPERAND_REG;
    inst->ternary[0].reg.name = ir_gen_expr(funcdef, ifstmnt->expr, NULL);
    inst->ternary[1].type = IR_OPERAND_LABEL;
    gen_appendinst(funcdef, inst);
    
    // then
    gen_newblock(funcdef);
    brinst->ternary[1].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
    ir_gen_statement(funcdef, ifstmnt->ifblk);

    // TODO: else

    // rest of function
    gen_newblock(funcdef);
    brinst->ternary[2].type = IR_OPERAND_LABEL;
    brinst->ternary[2].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
}

static void ir_gen_while(ir_funcdef_t* funcdef, whilestmnt_t* whilestmnt)
{
    int conditionblock;
    ir_inst_t *inst, *brinst;

    conditionblock = funcdef->blocks.len;
    gen_newblock(funcdef);
    // if true, jump to loop body
    brinst = inst = gen_allocinst();
    inst->op = IR_OP_BR;
    inst->ternary[0].type = IR_OPERAND_REG;
    inst->ternary[0].reg.name = ir_gen_expr(funcdef, whilestmnt->expr, NULL);
    inst->ternary[1].type = IR_OPERAND_LABEL;
    gen_appendinst(funcdef, inst);
    
    // body
    gen_newblock(funcdef);
    inst->ternary[1].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
    ir_gen_statement(funcdef, whilestmnt->body);
    // jump back to beginning
    inst = gen_allocinst();
    inst->op = IR_OP_JMP;
    inst->unary.type = IR_OPERAND_LABEL;
    inst->unary.label = strdup(funcdef->blocks.data[conditionblock].name);
    gen_appendinst(funcdef, inst);

    // rest of function
    gen_newblock(funcdef);
    brinst->ternary[2].type = IR_OPERAND_LABEL;
    brinst->ternary[2].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
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
    char *name;
    ir_inst_t *inst;
    strpair_t pair;

    name = ir_gen_alloctemp(funcdef, IR_PRIM_PTR);
    map_str_ir_reg_get(&funcdef->regs, name)->virtual = true;

    inst = gen_allocinst();
    inst->op = IR_OP_ALLOCA;
    inst->unary.type = IR_OPERAND_REG;
    inst->unary.reg.name = strdup(name);
    inst->alloca.type.type = IR_TYPE_PRIM;
    inst->alloca.type.prim = type_toprim(decl->type.type);
    gen_appendinst(funcdef, inst);

    pair.a = strdup(decl->ident);
    pair.b = name;
    list_strpair_ppush(&varnames, &pair);
}

static void ir_gen_arglist(ir_funcdef_t* funcdef, list_decl_t* arglist)
{
    int i;

    ir_inst_t *inst;
    char *reg;
    ir_param_t param;

    for(i=0; i<arglist->len; i++)
    {
        reg = ir_gen_alloctemp(funcdef, type_toprim(arglist->data[i].type.type));

        param.name = strdup(arglist->data[i].ident);
        param.loc.type = IR_LOCATION_REG;
        param.loc.reg = reg;
        list_ir_param_push(&funcdef->params, param);

        ir_gen_decl(funcdef, &arglist->data[i]);

        inst = gen_allocinst();
        inst->op = IR_OP_STORE;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(varnames.data[varnames.len-1].b);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = strdup(reg);
        gen_appendinst(funcdef, inst);
    }
}

static void ir_gen_globaldecl(globaldecl_t *globdecl)
{
    int i;

    ir_block_t blk;
    ir_funcdef_t funcdef;
    uint64_t oldvarstack;

    if(!globdecl->hasfuncdef)
        return;

    oldvarstack = varnames.len;

    insttail = NULL;

    funcdef.name = strdup(globdecl->decl.ident);
    funcdef.rettype.type = IR_TYPE_PRIM;
    funcdef.rettype.prim = type_toprim(globdecl->decl.type.type);
    list_ir_param_init(&funcdef.params, 0);
    funcdef.ntempreg = 0;
    list_ir_block_init(&funcdef.blocks, 1);
    map_str_u64_alloc(&funcdef.blktbl);
    map_str_ir_reg_alloc(&funcdef.regs);
    list_pir_block_init(&funcdef.postorder, 0);

    ir_initblock(&funcdef.blocks.data[0]);
    funcdef.blocks.data[0].name = strdup("entry");
    map_str_u64_set(&funcdef.blktbl, funcdef.blocks.data[0].name, 0);
    
    ir_gen_arglist(&funcdef, &globdecl->decl.args);

    for(i=0; i<globdecl->funcdef.decls.len; i++)
        ir_gen_decl(&funcdef, &globdecl->funcdef.decls.data[i]);
    gen_newblock(&funcdef);
    for(i=0; i<globdecl->funcdef.stmnts.len; i++)
        ir_gen_statement(&funcdef, &globdecl->funcdef.stmnts.data[i]);

    ir_initblock(&blk);
    blk.name = strdup("exit");
    list_ir_block_ppush(&funcdef.blocks, &blk);
    insttail = NULL;
    map_str_u64_set(&funcdef.blktbl, blk.name, funcdef.blocks.len - 1);

    list_ir_funcdef_ppush(&ir.defs, &funcdef);

    list_strpair_resize(&varnames, oldvarstack);
}

void gen(void)
{
    int i;

    list_strpair_init(&varnames, 0);

    list_ir_funcdef_init(&ir.defs, 0);
    for(i=0; i<ast.len; i++)
        ir_gen_globaldecl(&ast.data[i]);

    list_strpair_free(&varnames);
}

void ir_free(void)
{
    list_ir_funcdef_free(&ir.defs);
}
