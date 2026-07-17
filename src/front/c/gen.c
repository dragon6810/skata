#include "middle/ir.h"

#include <assert.h>
#include <stdio.h>

#include "ast.h"
#include "back/back.h"
#include "tag.h"

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
char* ir_gen_lvaladr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg);
static void ir_gen_structcpy(ir_funcdef_t* funcdef, type_t* type, char* dstadr, char* srcadr);
static void ir_gen_typeuse(type_t* type);

// you own returned string
static char* ir_gen_structtemp(ir_funcdef_t* funcdef, uint64_t agg)
{
    char *name;
    ir_inst_t *inst;
    ir_block_t *entry;

    name = ir_allocreg(funcdef, IR_PRIM_PTR);
    map_str_ir_reg_get(&funcdef->regs, name)->virtual = true;

    inst = gen_allocinst();
    inst->op = IR_OP_ALLOCA;
    inst->unary.type = IR_OPERAND_REG;
    inst->unary.reg.name = strdup(name);
    inst->alloca.type.type = IR_TYPE_AGG;
    inst->alloca.type.agg = agg;

    entry = &funcdef->blocks.data[0];
    inst->next = entry->insts;
    entry->insts = inst;

    return name;
}

// the address of a struct-typed expression, whether it is an lvalue (a
// variable/member) or an rvalue (a call result). you own the returned string.
static char* ir_gen_structaddr(ir_funcdef_t* funcdef, expr_t* expr)
{
    return expr->lval ? ir_gen_lvaladr(funcdef, expr, NULL)
                      : ir_gen_expr(funcdef, expr, NULL);
}

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
    inst->ternary[0].reg.name = cmpres = ir_allocreg(funcdef, IR_PRIM_U1);
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
    res = outreg ? outreg : ir_allocreg(funcdef, type_toprim(expr->operands[1]->type.type));
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
    expr_t *arg;
    char *copy, *src;

    inst = gen_allocinst();
    inst->op = IR_OP_CALL;
    list_ir_operand_init(&inst->variadic, 2);

    inst->variadic.data[1].type = IR_OPERAND_SYMBOL;
    inst->variadic.data[1].sym = strdup(expr->operand->msg);

    for(i=0; i<expr->variadic.len; i++)
    {
        arg = expr->variadic.data[i];
        operand.type = IR_OPERAND_REG;

        if(arg->type.type == TYPE_STRUCT)
        {
            ir_gen_typeuse(&arg->type);
            copy = ir_gen_structtemp(funcdef, arg->type.struc.agg);
            src = ir_gen_structaddr(funcdef, arg);
            ir_gen_structcpy(funcdef, &arg->type, copy, src);
            free(src);
            operand.reg.name = copy;
        }
        else
            operand.reg.name = ir_gen_expr(funcdef, arg, NULL);

        list_ir_operand_push(&inst->variadic, operand);
    }

    if(expr->type.type == TYPE_STRUCT)
    {
        ir_gen_typeuse(&expr->type);
        res = ir_gen_structtemp(funcdef, expr->type.struc.agg);
    }
    else if(outreg)
        res = outreg;
    else
        res = ir_allocreg(funcdef, type_toprim(expr->type.type));

    inst->variadic.data[0].type = IR_OPERAND_REG;
    inst->variadic.data[0].reg.name = strdup(res);

    if(expr->type.type == TYPE_STRUCT)
    {
        inst->call.rettype.type = IR_TYPE_AGG;
        inst->call.rettype.agg = expr->type.struc.agg;
    }
    else
    {
        inst->call.rettype.type = IR_TYPE_PRIM;
        inst->call.rettype.prim = type_toprim(expr->type.type);
    }

    list_ir_type_init(&inst->call.argtypes, expr->variadic.len);
    for(i=0; i<expr->variadic.len; i++)
    {
        arg = expr->variadic.data[i];
        if(arg->type.type == TYPE_STRUCT)
        {
            inst->call.argtypes.data[i].type = IR_TYPE_AGG;
            inst->call.argtypes.data[i].agg = arg->type.struc.agg;
        }
        else
        {
            inst->call.argtypes.data[i].type = IR_TYPE_PRIM;
            inst->call.argtypes.data[i].prim = type_toprim(arg->type.type);
        }
    }

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
    
    res = outreg ? outreg : ir_allocreg(funcdef, dsttype);

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
list_pdecl_t vardecls; // indexes match with varnames

// return string belongs to the varnames
static char* ir_gen_varname(const char* cname)
{
    int i;

    for(i=varnames.len-1; i>=0; i--)
        if(!strcmp(cname, varnames.data[i].a))
            return varnames.data[i].b;

    assert(0);
    return NULL;
}

// an array variable decays to a pointer whose value is its own address,
// so it is read without a load
static bool ir_gen_vararray(const char* cname)
{
    int i;

    for(i=varnames.len-1; i>=0; i--)
        if(!strcmp(cname, varnames.data[i].a))
            return vardecls.data[i]->type.type == TYPE_ARR;

    assert(0);
    return false;
}

// i think its fine to have a bunch of asserts here because if the front end works
// the errors would be found during semantic analysis
// returns the aggregate id
static uint64_t ir_gen_findfid(const type_t* aggtype, const char* member, ir_fid_t* outfid)
{
    int fid, typeid;

    struct_t *def;

    assert(aggtype->type == TYPE_STRUCT);

    def = NULL;
    if(aggtype->struc.def)
        def = aggtype->struc.def;
    if(aggtype->struc.tag && aggtype->struc.tag->defined)
        def = &aggtype->struc.tag->struc;
    assert(def);

    for(fid=0; fid<def->members.len; fid++)
    {
        // if you encounter a nameless union, youd end up wanting to increment typeid
        // for its members
        typeid = 0;
        if(!strcmp(def->members.data[fid].ident, member))
            break;
    }
    assert(fid < def->members.len);

    outfid->fid = fid;
    outfid->typeid = typeid;

    return def->hash;
}

char* ir_gen_lvaladr(ir_funcdef_t *funcdef, expr_t *expr, char* outreg)
{
    ir_inst_t *inst;
    char *res, *adrname;
    uint64_t typesize;

    assert(expr->lval);
    assert(expr->op == EXPROP_VAR || expr->op == EXPROP_DEREF || expr->op == EXPROP_MEMBER || expr->op == EXPROP_INDEX);

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
    if(expr->op == EXPROP_MEMBER)
    {
        res = outreg ? outreg : ir_allocreg(funcdef, IR_PRIM_PTR);

        inst = gen_allocinst();
        inst->op = IR_OP_FIDADR;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(res);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = ir_gen_structaddr(funcdef, expr->operands[0]);
        list_ir_fid_init(&inst->fid.fids, 1);
        inst->fid.agg = ir_gen_findfid(&expr->operands[0]->type, expr->member, &inst->fid.fids.data[0]);

        gen_appendinst(funcdef, inst);

        return res;
    }
    if(expr->op == EXPROP_INDEX)
    {
        res = outreg ? outreg : ir_allocreg(funcdef, IR_PRIM_PTR);

        switch(expr->operands[0]->type.ptrtype->type)
        {
        case TYPE_STRUCT:
            typesize = back_aggbytesize(expr->operands[0]->type.ptrtype->struc.agg);
            break;
        default:
            typesize = ir_primbytesize(type_toprim(expr->operands[0]->type.ptrtype->type));
            break;
        }

        inst = gen_allocinst();
        inst->op = IR_OP_MUL;
        inst->ternary[0].type = IR_OPERAND_REG;
        inst->ternary[0].reg.name = adrname = ir_allocreg(funcdef, IR_PRIM_PTR);
        inst->ternary[1].type = IR_OPERAND_LIT;
        inst->ternary[1].literal.type = IR_PRIM_PTR;
        inst->ternary[1].literal.ptr = typesize;
        inst->ternary[2].type = IR_OPERAND_REG;
        inst->ternary[2].reg.name = ir_gen_expr(funcdef, expr->operands[1], NULL);
        gen_appendinst(funcdef, inst);

        inst = gen_allocinst();
        inst->op = IR_OP_ADD;
        inst->ternary[0].type = IR_OPERAND_REG;
        inst->ternary[0].reg.name = strdup(res);
        inst->ternary[1].type = IR_OPERAND_REG;
        // the base is the pointer VALUE of operands[0]: a decayed array
        // yields its address, a real pointer yields its loaded value
        inst->ternary[1].reg.name = ir_gen_expr(funcdef, expr->operands[0], NULL);
        inst->ternary[2].type = IR_OPERAND_REG;
        inst->ternary[2].reg.name = strdup(adrname);
        gen_appendinst(funcdef, inst);

        return res;
    }

    return ir_gen_expr(funcdef, expr->operand, outreg);
}

char* ir_gen_member(ir_funcdef_t* funcdef, expr_t* expr, char* outreg)
{
    char *res;
    ir_inst_t *inst;
    
    res = outreg ? outreg : ir_allocreg(funcdef, type_toprim(expr->type.type));

    inst = gen_allocinst();
    inst->op = IR_OP_LOAD;
    inst->binary[0].type = IR_OPERAND_REG;
    inst->binary[0].reg.name = strdup(res);
    inst->binary[1].type = IR_OPERAND_REG;
    inst->binary[1].reg.name = ir_gen_lvaladr(funcdef, expr, NULL);

    gen_appendinst(funcdef, inst);

    return res;
}

static char* ir_gen_fidadr(ir_funcdef_t* funcdef, uint64_t agg, uint64_t fid, char* base)
{
    char *res;
    ir_inst_t *inst;

    res = ir_allocreg(funcdef, IR_PRIM_PTR);

    inst = gen_allocinst();
    inst->op = IR_OP_FIDADR;
    inst->binary[0].type = IR_OPERAND_REG;
    inst->binary[0].reg.name = strdup(res);
    inst->binary[1].type = IR_OPERAND_REG;
    inst->binary[1].reg.name = strdup(base);
    list_ir_fid_init(&inst->fid.fids, 1);
    inst->fid.fids.data[0].fid = fid;
    inst->fid.fids.data[0].typeid = 0;
    inst->fid.agg = agg;
    gen_appendinst(funcdef, inst);

    return res;
}

static void ir_gen_structcpy(ir_funcdef_t* funcdef, type_t* type, char* dstadr, char* srcadr)
{
    int i;

    struct_t *def;
    type_t *membertype;
    char *mdst, *msrc, *tmp;
    ir_inst_t *inst;

    def = struct_finddef(type);
    assert(def);

    for(i=0; i<def->members.len; i++)
    {
        membertype = &def->members.data[i].type;

        msrc = ir_gen_fidadr(funcdef, def->hash, i, srcadr);
        mdst = ir_gen_fidadr(funcdef, def->hash, i, dstadr);

        if(membertype->type == TYPE_STRUCT)
        {
            ir_gen_structcpy(funcdef, membertype, mdst, msrc);
            free(msrc);
            free(mdst);
            continue;
        }

        tmp = ir_allocreg(funcdef, type_toprim(membertype->type));

        inst = gen_allocinst();
        inst->op = IR_OP_LOAD;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(tmp);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = msrc;
        gen_appendinst(funcdef, inst);

        inst = gen_allocinst();
        inst->op = IR_OP_STORE;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = mdst;
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = tmp;
        gen_appendinst(funcdef, inst);
    }
}

char* ir_gen_structassign(ir_funcdef_t *funcdef, expr_t *expr, char* outreg)
{
    char *dst, *src;
    ir_inst_t *inst;

    assert(struct_finddef(&expr->operands[0]->type)->hash == struct_finddef(&expr->operands[1]->type)->hash);

    if(expr->operands[1]->lval)
        src = ir_gen_lvaladr(funcdef, expr->operands[1], NULL);
    else
        src = ir_gen_expr(funcdef, expr->operands[1], NULL);
    dst = ir_gen_lvaladr(funcdef, expr->operands[0], NULL);

    ir_gen_structcpy(funcdef, &expr->operands[0]->type, dst, src);

    free(src);

    // the value of the assignment is the lhs, so give back the address
    if(!outreg)
        return dst;

    inst = gen_allocinst();
    inst->op = IR_OP_MOVE;
    inst->binary[0].type = IR_OPERAND_REG;
    inst->binary[0].reg.name = strdup(outreg);
    inst->binary[1].type = IR_OPERAND_REG;
    inst->binary[1].reg.name = dst;
    gen_appendinst(funcdef, inst);

    return outreg;
}

static uint64_t nstringliterals;

char* ir_gen_stringlit(ir_funcdef_t *funcdef, expr_t *expr, char* outreg)
{
    ir_inst_t *inst;
    int len;
    ir_data_t data;

    assert(expr->op == EXPROP_STRING);

    inst = gen_allocinst();
    inst->op = IR_OP_SYMADR;
    inst->binary[0].type = IR_OPERAND_REG;
    inst->binary[0].reg.name = strdup(outreg);
    inst->binary[1].type = IR_OPERAND_SYMBOL;
    len = snprintf(NULL, 0, ".str.%llu", nstringliterals) + 1;
    inst->binary[1].sym = malloc(len + 1);
    snprintf(inst->binary[1].sym, len, ".str.%llu", nstringliterals);
    gen_appendinst(funcdef, inst);

    data.dontlink = true;
    data.constant = true;
    data.name = strdup(inst->binary[1].sym);
    // TODO: escape sequences and stuff
    list_u8_init(&data.data, strlen(expr->msg) + 1);
    strcpy((char*) data.data.data, expr->msg);
    map_str_ir_data_set(&ir.data, data.name, data);
    ir_freedata(&data);

    nstringliterals++;
    return outreg;
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
    {
        if(expr->type.type == TYPE_STRUCT)
            res = ir_allocreg(funcdef, IR_PRIM_PTR);
        else
            res = ir_allocreg(funcdef, type_toprim(expr->type.type));
    }

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
        // an array decays to its address; everything else loads its value
        inst = gen_allocinst();
        inst->op = ir_gen_vararray(expr->msg) ? IR_OP_MOVE : IR_OP_LOAD;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(res);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = strdup(ir_gen_varname(expr->msg));
        gen_appendinst(funcdef, inst);
        return res;
    case EXPROP_STRING:
        return ir_gen_stringlit(funcdef, expr, res);
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
    case EXPROP_EQ:
        opcode = IR_OP_CMPEQ;
        break;
    case EXPROP_NEQ:
        opcode = IR_OP_CMPNEQ;
        break;
    case EXPROP_ASSIGN:
        if(expr->type.type == TYPE_STRUCT)
            return ir_gen_structassign(funcdef, expr, res);
        
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
    case EXPROP_DEREF:
        inst = gen_allocinst();
        inst->op = IR_OP_LOAD;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(res);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = ir_gen_expr(funcdef, expr->operand, NULL);
        gen_appendinst(funcdef, inst);
        return res;
    case EXPROP_INDEX:
        // load from base + index*typesize; ir_gen_lvaladr handles the offset,
        // whereas expr->operand would alias operands[0] (the base) and drop the index
        inst = gen_allocinst();
        inst->op = IR_OP_LOAD;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(res);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = ir_gen_lvaladr(funcdef, expr, NULL);
        gen_appendinst(funcdef, inst);
        return res;
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
    case EXPROP_MEMBER:
        return ir_gen_member(funcdef, expr, outreg);
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
    char *src;
    
    if(expr && funcdef->rettype.type == IR_TYPE_AGG)
    {
        src = ir_gen_structaddr(funcdef, expr);
        ir_gen_structcpy(funcdef, &expr->type, funcdef->retreg, src);
        free(src);

        inst = gen_allocinst();
        inst->op = IR_OP_RET;
        inst->hasval = false;
        gen_appendinst(funcdef, inst);
        return;
    }

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
    ir_inst_t *inst, *brinst, *jmpinst;

    // branch
    brinst = inst = gen_allocinst();
    inst->op = IR_OP_BR;
    inst->ternary[0].type = IR_OPERAND_REG;
    inst->ternary[0].reg.name = ir_gen_expr(funcdef, ifstmnt->expr, NULL);
    inst->ternary[1].type = inst->ternary[2].type = IR_OPERAND_LABEL;
    gen_appendinst(funcdef, inst);
    
    // then
    gen_newblock(funcdef);
    brinst->ternary[1].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
    ir_gen_statement(funcdef, ifstmnt->ifblk);
    jmpinst = gen_allocinst();
    jmpinst->op = IR_OP_JMP;
    jmpinst->unary.type = IR_OPERAND_LABEL;
    gen_appendinst(funcdef, jmpinst);

    // else
    gen_newblock(funcdef);
    brinst->ternary[2].label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
    if(ifstmnt->elseblk)
        ir_gen_statement(funcdef, ifstmnt->elseblk);

    // rest of function
    gen_newblock(funcdef);
    jmpinst->unary.label = strdup(funcdef->blocks.data[funcdef->blocks.len-1].name);
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

static void ir_gen_typeuse(type_t* type);

static void ir_gen_decl(ir_funcdef_t* funcdef, decl_t* decl)
{
    char *name;
    ir_inst_t *inst;
    strpair_t pair;
    int arrsize;
    type_t *curtype;

    ir_gen_typeuse(&decl->type);

    name = ir_allocreg(funcdef, IR_PRIM_PTR);
    map_str_ir_reg_get(&funcdef->regs, name)->virtual = true;

    inst = gen_allocinst();
    inst->op = IR_OP_ALLOCA;
    inst->binary[0].type = IR_OPERAND_REG;
    inst->binary[0].reg.name = strdup(name);
    arrsize = 1;
    switch(decl->type.type)
    {
    case TYPE_STRUCT:
        inst->alloca.type.type = IR_TYPE_AGG;
        inst->alloca.type.agg = decl->type.struc.agg;
        break;
    case TYPE_ARR:
        arrsize = decl->type.arr.size;
        curtype = decl->type.arr.type;
        while(curtype->type == TYPE_ARR)
        {
            arrsize *= curtype->arr.size;
            curtype = curtype->arr.type;
        }
        if(curtype->type == TYPE_STRUCT)
        {
            inst->alloca.type.type = IR_TYPE_AGG;
            inst->alloca.type.agg = curtype->struc.agg;
        }
        else
        {
            inst->alloca.type.type = IR_TYPE_PRIM;
            inst->alloca.type.prim = type_toprim(curtype->type);
        }
        break;
    default:
        inst->alloca.type.type = IR_TYPE_PRIM;
        inst->alloca.type.prim = type_toprim(decl->type.type);
        break;
    }

    inst->binary[1].type = IR_OPERAND_LIT;
    inst->binary[1].literal.type = IR_PRIM_U64;
    inst->binary[1].literal.u64 = arrsize;
    
    gen_appendinst(funcdef, inst);

    pair.a = strdup(decl->ident);
    pair.b = name;
    list_strpair_ppush(&varnames, &pair);
    list_pdecl_push(&vardecls, decl);
}

static void ir_gen_arglist(ir_funcdef_t* funcdef, list_decl_t* arglist)
{
    int i;

    ir_inst_t *inst;
    char *reg;
    ir_param_t param;
    strpair_t pair;
    decl_t *arg;

    for(i=0; i<arglist->len; i++)
    {
        arg = &arglist->data[i];

        if(arg->type.type == TYPE_STRUCT)
        {
            ir_gen_typeuse(&arg->type);

            reg = ir_allocreg(funcdef, IR_PRIM_PTR);
            param.type.type = IR_TYPE_AGG;
            param.type.agg = arg->type.struc.agg;
            param.reg = reg;
            list_ir_param_push(&funcdef->params, param);

            pair.a = strdup(arg->ident);
            pair.b = strdup(reg);
            list_strpair_ppush(&varnames, &pair);
            list_pdecl_push(&vardecls, arg);
            continue;
        }

        reg = ir_allocreg(funcdef, type_toprim(arg->type.type));

        param.type.type = IR_TYPE_PRIM;
        param.type.prim = type_toprim(arg->type.type);
        param.reg = reg;
        list_ir_param_push(&funcdef->params, param);

        ir_gen_decl(funcdef, arg);

        inst = gen_allocinst();
        inst->op = IR_OP_STORE;
        inst->binary[0].type = IR_OPERAND_REG;
        inst->binary[0].reg.name = strdup(varnames.data[varnames.len-1].b);
        inst->binary[1].type = IR_OPERAND_REG;
        inst->binary[1].reg.name = strdup(reg);
        gen_appendinst(funcdef, inst);
    }
}

static void ir_gen_typeuse(type_t* type)
{
    int i;

    struct_t *def;
    type_t *membertype;
    ir_aggregate_t agg;
    ir_aggfid_t fid;
    ir_type_t irtype;

    if(type->type != TYPE_STRUCT)
        return;

    def = struct_finddef(type);
    assert(def);

    type->struc.agg = def->hash;

    if(map_u64_ir_aggregate_get(&ir.aggs, def->hash))
        return;

    agg.type = AGG_STRUCT;
    list_ir_aggfid_init(&agg.struc.fids, def->members.len);
    for(i=0; i<def->members.len; i++)
    {
        membertype = &def->members.data[i].type;
        ir_gen_typeuse(membertype);

        if(membertype->type == TYPE_STRUCT)
        {
            irtype.type = IR_TYPE_AGG;
            irtype.agg = membertype->struc.agg;
        }
        else
        {
            irtype.type = IR_TYPE_PRIM;
            irtype.prim = type_toprim(membertype->type);
        }

        list_ir_type_init(&fid.types, 1);
        fid.types.data[0] = irtype;
        agg.struc.fids.data[i] = fid;
    }

    map_u64_ir_aggregate_set(&ir.aggs, def->hash, agg);
    ir_aggregatefree(&agg);
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

    ir_gen_typeuse(&globdecl->decl.type);

    funcdef.name = strdup(globdecl->decl.ident);
    switch(globdecl->decl.type.type)
    {
    case TYPE_STRUCT:
        funcdef.rettype.type = IR_TYPE_AGG;
        funcdef.rettype.agg = globdecl->decl.type.struc.agg;
        break;
    default:
        funcdef.rettype.type = IR_TYPE_PRIM;
        funcdef.rettype.prim = type_toprim(globdecl->decl.type.type);
        break;
    }
    funcdef.retreg = NULL;
    list_ir_param_init(&funcdef.params, 0);
    funcdef.ntempreg = 0;
    list_ir_block_init(&funcdef.blocks, 1);
    map_str_u64_alloc(&funcdef.blktbl);
    map_str_ir_reg_alloc(&funcdef.regs);
    list_pir_block_init(&funcdef.postorder, 0);

    // aggregate returns get an implicit incoming pointer to write through
    if(funcdef.rettype.type == IR_TYPE_AGG)
        funcdef.retreg = ir_allocreg(&funcdef, IR_PRIM_PTR);

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
    list_pdecl_resize(&vardecls, oldvarstack);
}

void gen(void)
{
    int i;

    nstringliterals = 0;

    list_strpair_init(&varnames, 0);
    list_pdecl_init(&vardecls, 0);

    list_ir_funcdef_init(&ir.defs, 0);
    map_u64_ir_aggregate_alloc(&ir.aggs);
    map_str_ir_data_alloc(&ir.data);
    for(i=0; i<ast.len; i++)
        ir_gen_globaldecl(&ast.data[i]);

    list_pdecl_free(&vardecls);
    list_strpair_free(&varnames);
}

void ir_free(void)
{
    map_str_ir_data_free(&ir.data);
    map_u64_ir_aggregate_free(&ir.aggs);
    list_ir_funcdef_free(&ir.defs);
}
