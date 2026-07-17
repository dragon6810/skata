#include <assert.h>
#include <stdio.h>

#include "ast.h"
#include "front/error.h"
#include "struct.h"
#include "tag.h"

list_pdecl_t scope;

const static type_t semantics_inttype = { .type = TYPE_I32 };

static type_e semantics_getliteraltype(uint64_t lit)
{
    if(lit <= INT32_MAX)
        return TYPE_I32;
    if(lit <= UINT32_MAX)
        return TYPE_U32;
    if(lit <= INT64_MAX)
        return TYPE_I64;
    return TYPE_U64;
}

static bool semantics_typecmp(const type_t* a, const type_t* b)
{
    int i;

    if(a->type != b->type)
        return false;

    switch(a->type)
    {
    case TYPE_PTR:
        if(!a->ptrtype && !b->ptrtype)
            break;
        if(!a->ptrtype || !b->ptrtype)
            return false;
        return semantics_typecmp(a->ptrtype, b->ptrtype);
    case TYPE_FUNC:
        if(!semantics_typecmp(a->func.ret, b->func.ret))
            return false;
        if(a->func.args.len != b->func.args.len)
            return false;
        for(i=0; i<a->func.args.len; i++)
            if(!semantics_typecmp(&a->func.args.data[i], &b->func.args.data[i]))
                return false;
        break;
    case TYPE_STRUCT:
        if(a->struc.tag != b->struc.tag)
            return false;
        break;
    default:
        break;
    }

    return true;
}

static void semantics_cast(type_t type, expr_t* expr)
{
    expr_t *newexpr;

    assert(expr);

    if(semantics_typecmp(&expr->type, &type))
        return;

    newexpr = malloc(sizeof(expr_t));
    *newexpr = *expr;

    expr->op = EXPROP_CAST;
    expr->operand = newexpr;
    type_cpy(&expr->type, &type);
    type_cpy(&expr->casttype, &type);
}

static void semantics_expr(expr_t* expr);

static void semantics_truthybinaryexpr(expr_t* expr)
{
    int i;

    for(i=0; i<2; i++)
    {
        semantics_expr(expr->operands[i]);
        if(expr->operands[i]->type.type < TYPE_I32)
            semantics_cast(semantics_inttype, expr->operands[i]);
    }

    expr->type.type = TYPE_U1;
}

static void semantics_binaryexpr(expr_t* expr)
{
    int i;

    for(i=0; i<2; i++)
    {
        semantics_expr(expr->operands[i]);
        if(expr->operands[i]->type.type < TYPE_I32)
            semantics_cast(semantics_inttype, expr->operands[i]);
    }

    if(expr->operands[1]->type.type > expr->operands[0]->type.type)
    {
        semantics_cast(expr->operands[1]->type, expr->operands[0]);
        type_cpy(&expr->type, &expr->operands[1]->type);
    }
    else
    {
        semantics_cast(expr->operands[0]->type, expr->operands[1]);
        type_cpy(&expr->type, &expr->operands[0]->type);
    }
}

static void semantics_truthify(expr_t* expr)
{
    type_t inttype;
    expr_t *oldexpr, *zero;

    if(expr->type.type == TYPE_U1)
        return;

    if(expr->type.type < TYPE_I32)
    {
        inttype.type = TYPE_I32;
        semantics_cast(inttype, expr);
    }

    zero = malloc(sizeof(expr_t));
    zero->op = EXPROP_LIT;
    zero->type.type = expr->type.type;
    zero->u64 = 0;

    oldexpr = malloc(sizeof(expr_t));
    memcpy(oldexpr, expr, sizeof(expr_t));

    expr->op = EXPROP_NEQ;
    expr->operands[0] = oldexpr;
    expr->operands[1] = zero;
    expr->type.type = TYPE_U1;
}

static void semantics_condition(expr_t* expr)
{
    semantics_expr(expr);
    semantics_truthify(expr);
}

static void semantics_ternaryexpr(expr_t* expr)
{
    int i;

    semantics_condition(expr->operands[0]);

    for(i=1; i<3; i++)
    {
        semantics_expr(expr->operands[i]);
        if(expr->operands[i]->type.type < TYPE_I32)
            semantics_cast(semantics_inttype, expr->operands[i]);
    }

    expr->type.type = TYPE_I32;
    for(i=1; i<3; i++)
        if(expr->operands[i]->type.type > expr->type.type)
            expr->type.type = expr->operands[i]->type.type;
}

static void semantics_assignexpr(expr_t* expr)
{
    int i;

    assert(expr);

    for(i=0; i<2; i++)
        semantics_expr(expr->operands[i]);

    if(!expr->operands[0]->lval)
        error(true, expr->line, expr->col, "expression is not assignable\n");

    expr->type = expr->operands[0]->type;
    if(!semantics_typecmp(&expr->type, &expr->operands[1]->type))
        semantics_cast(expr->type, expr->operands[1]);
}

static void semantics_logicalnot(expr_t* expr)
{
    semantics_truthify(expr->operand);
    expr->type.type = TYPE_U1;
}

static void semantics_derefexpr(expr_t* expr)
{
    semantics_expr(expr->operand);
    if(expr->operand->type.type != TYPE_PTR)
        error(true, expr->line, expr->col, "expression must be a pointer\n");

    type_cpy(&expr->type, expr->operand->type.ptrtype);

    expr->lval = true;
}

static void semantics_refexpr(expr_t* expr)
{
    semantics_expr(expr->operand);
    if(!expr->operand->lval)
        error(true, expr->line, expr->col, "expression must be an lval\n");

    expr->type.type = TYPE_PTR;
    expr->type.ptrtype = malloc(sizeof(type_t));
    type_cpy(expr->type.ptrtype, &expr->operand->type);
}

static void semantics_unaryexpr(expr_t* expr)
{
    semantics_expr(expr->operand);
    if(expr->operand->type.type > TYPE_I32)
        expr->type.type = expr->operand->type.type;
    else
        expr->type.type = TYPE_I32;
}

// unary no-op, e.g. unary +
static void semantics_noop(expr_t* expr)
{
    expr_t* child;

    semantics_expr(expr->operand);
    child = expr->operand;

    memcpy(expr, child, sizeof(expr_t));
    free(child);
}

static void semantics_castop(expr_t* expr)
{
    semantics_expr(expr->operand);
    expr->type = expr->casttype;
}

static void semantics_callexpr(expr_t* expr)
{
    int i;

    type_t *functype;

    assert(expr->variadic.len);

    semantics_expr(expr->operand);

    functype = &expr->operand->type;
    if(functype->type != TYPE_FUNC)
        error(true, expr->operand->line, expr->operand->col, "expected function or function pointer\n");

    if(expr->variadic.len < functype->func.args.len)
        error(true, expr->line, expr->col, "too few arguments to function call, expected %d\n");
    if(expr->variadic.len > functype->func.args.len)
        error(true, expr->line, expr->col, "too many arguments to function call\n");

    for(i=0; i<functype->func.args.len; i++)
    {
        semantics_expr(expr->variadic.data[i]);
        semantics_cast(functype->func.args.data[i], expr->variadic.data[i]);
    }

    type_cpy(&expr->type, functype->func.ret);
}

static void semantics_memberexpr(expr_t* expr)
{
    int i;

    const char* name;
    struct_t *def;

    assert(expr->op == EXPROP_MEMBER);

    expr->lval = true;

    semantics_expr(expr->operand);
    if(expr->operand->type.type != TYPE_STRUCT)
        error(true, expr->line, expr->col, "expression must have struct type\n");

    if(!expr->operand->type.struc.tag)
        name = "<anonymous>";
    else
        name = expr->operand->type.struc.tag->tag;

    def = struct_finddef(&expr->operand->type);
    if((expr->operand->type.struc.tag && !expr->operand->type.struc.tag->defined) && !expr->operand->type.struc.def)
        error(true, expr->line, expr->col, "use of incomplete struct %s\n", name);

    for(i=0; i<def->members.len; i++)
    {
        if(strcmp(def->members.data[i].ident, expr->member))
            continue;
        type_cpy(&expr->type, &def->members.data[i].type);
        return;
    }

    error(true, expr->line, expr->col, "struct %s has no member '%s'\n", name, expr->member);
}

static void semantics_indexexpr(expr_t* expr)
{
    expr_t *castexpr;

    assert(expr->op == EXPROP_INDEX);

    expr->lval = true;

    semantics_expr(expr->operands[0]);
    if(expr->operands[0]->type.type != TYPE_PTR)
        error(true, expr->operands[0]->line, expr->operands[0]->col, "expression must have pointer type\n");

    semantics_expr(expr->operands[1]);
    switch(expr->operands[1]->type.type)
    {
    case TYPE_U1:
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
        break;
    default:
        error(true, expr->operands[1]->line, expr->operands[1]->col, "expression must have integral type\n");
        break;
    }

    type_cpy(&expr->type, expr->operands[0]->type.ptrtype);

    // in the ir indexing is done with a multiply that needs two operands of same type.
    // cast to pointer now for simplicity in gen.
    castexpr = calloc(1, sizeof(expr_t));
    castexpr->op = EXPROP_CAST;
    castexpr->operand = expr->operands[1];
    castexpr->type.type = TYPE_PTR;
    castexpr->type.ptrtype = calloc(1, sizeof(expr_t));
    castexpr->type.ptrtype->type = TYPE_VOID;
    castexpr->casttype.type = TYPE_PTR;
    castexpr->casttype.ptrtype = calloc(1, sizeof(expr_t));
    castexpr->casttype.ptrtype->type = TYPE_VOID;

    expr->operands[1] = castexpr;
}

static void semantics_var(expr_t* expr)
{
    int i, j;

    type_t type;

    assert(expr->op == EXPROP_VAR);
    assert(scope.len);

    // more narrow scopes get precedence,
    // so loop from top to bottom
    for(i=scope.len-1; i>=0; i--)
    {
        if(!strcmp(expr->msg, scope.data[i]->ident))
            break;
    }
    
    if(i<0)
    {
        error(true, expr->line, expr->col, "use of undeclared identifier '%s'\n", expr->msg);
        exit(1);
    }

    if(scope.data[i]->form == DECL_FUNC)
    {
        expr->type.type = TYPE_FUNC;
        
        expr->type.func.ret = malloc(sizeof(type_t));
        type_cpy(expr->type.func.ret, &scope.data[i]->type);

        list_type_init(&expr->type.func.args, 0);
        for(j=0; j<scope.data[i]->args.len; j++)
        {
            type_cpy(&type, &scope.data[i]->args.data[j].type);
            list_type_ppush(&expr->type.func.args, &type);
        }

        expr->lval = false;
        return;
    }

    if(scope.data[i]->form == DECL_VAR && scope.data[i]->type.type == TYPE_ARR)
    {
        expr->type.type = TYPE_PTR;
        
        expr->type.ptrtype = malloc(sizeof(type_t));
        type_cpy(expr->type.ptrtype, scope.data[i]->type.arr.type);

        expr->lval = false;
        return;
    }

    type_cpy(&expr->type, &scope.data[i]->type);
    expr->lval = true;
}

static void semantics_expr(expr_t* expr)
{
    if(!expr)
        return;

    expr->lval = false;

    switch(expr->op)
    {
    case EXPROP_LIT:
        expr->type.type = semantics_getliteraltype(expr->u64);
        break;
    case EXPROP_VAR:
        semantics_var(expr);
        break;
    case EXPROP_STRING:
        // TODO: make const ptr when i add that
        expr->type.type = TYPE_PTR;
        expr->type.ptrtype = calloc(1, sizeof(type_t));
        expr->type.ptrtype->type = TYPE_I8;
        break;
    case EXPROP_COND:
        semantics_ternaryexpr(expr);
        break;
    case EXPROP_ASSIGN:
        semantics_assignexpr(expr);
        break;
    case EXPROP_ADD:
    case EXPROP_SUB:
    case EXPROP_MULT:
    case EXPROP_DIV:
        semantics_binaryexpr(expr);
        break;
    case EXPROP_EQ:
    case EXPROP_NEQ:
        semantics_truthybinaryexpr(expr);
        break;
    case EXPROP_NEG:
    case EXPROP_PREINC:
    case EXPROP_PREDEC:
    case EXPROP_POSTINC:
    case EXPROP_POSTDEC:
        semantics_unaryexpr(expr);
        break;
    case EXPROP_REF:
        semantics_refexpr(expr);
        break;
    case EXPROP_DEREF:
        semantics_derefexpr(expr);
        break;
    case EXPROP_LOGICNOT:
        semantics_logicalnot(expr);
        break;
    case EXPROP_POS:
        semantics_noop(expr);
        break;
    case EXPROP_CAST:
        semantics_castop(expr);
        break;
    case EXPROP_CALL:
        semantics_callexpr(expr);
        break;
    case EXPROP_MEMBER:
        semantics_memberexpr(expr);
        break;
    case EXPROP_INDEX:
        semantics_indexexpr(expr);
        break;
    default:
        assert(0);
        break;
    }
}

static void semantics_return(globaldecl_t* func, stmnt_t* stmnt)
{
    semantics_expr(stmnt->expr);
    semantics_cast(func->decl.type, stmnt->expr);
}

static void semantics_compound(globaldecl_t* func, compound_t* compound);

static void semantics_statement(globaldecl_t* func, stmnt_t* stmnt)
{
    switch(stmnt->form)
    {
    case STMNT_EXPR:
        semantics_expr(stmnt->expr);
        break;
    case STMNT_COMPOUND:
        semantics_compound(func, &stmnt->compound);
        break;
    case STMNT_RETURN:
        semantics_return(func, stmnt);
        break;
    case STMNT_IF:
        semantics_condition(stmnt->ifstmnt.expr);
        semantics_statement(func, stmnt->ifstmnt.ifblk);
        if(stmnt->ifstmnt.elseblk)
            semantics_statement(func, stmnt->ifstmnt.elseblk);
        break;
    case STMNT_WHILE:
        semantics_condition(stmnt->whilestmnt.expr);
        semantics_statement(func, stmnt->whilestmnt.body);
        break;
    default:
        break;
    }
}

static void semantics_decl(decl_t* decl);

static void semantics_compound(globaldecl_t* func, compound_t* compound)
{
    int i;

    uint64_t scopesize;

    scopesize = scope.len;

    for(i=0; i<compound->decls.len; i++)
    {
        list_pdecl_push(&scope, &compound->decls.data[i]);
        semantics_decl(&compound->decls.data[i]);
    }

    for(i=0; i<compound->stmnts.len; i++)
        semantics_statement(func, &compound->stmnts.data[i]);

    list_pdecl_resize(&scope, scopesize);
}

static void semantics_funcdef(globaldecl_t* func)
{
    int i;

    uint64_t scopesize;

    list_pdecl_push(&scope, &func->decl);

    scopesize = scope.len;
    for(i=0; i<func->decl.args.len; i++)
        list_pdecl_push(&scope, &func->decl.args.data[i]);
    semantics_compound(func, &func->funcdef);
    list_pdecl_resize(&scope, scopesize);
}

static void semantics_decl(decl_t* decl)
{
    if(decl->expr)
        semantics_expr(decl->expr);
}

void semantics(void)
{
    int i;

    list_pdecl_init(&scope, 0);

    for(i=0; i<ast.len; i++)
    {
        semantics_decl(&ast.data[i].decl);
        if(ast.data[i].hasfuncdef)
            semantics_funcdef(&ast.data[i]);
    }

    list_pdecl_free(&scope);
}