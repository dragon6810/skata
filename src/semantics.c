#include "semantics.h"

#include <stdio.h>

#include "ast.h"
#include "error.h"

list_pdecl_t scope;

static type_t semantics_getvariabletype(expr_t* expr)
{
    int i;

    assert(expr->op == EXPROP_VAR);

    // more narrow scopes get precedence,
    // so loop from top to bottom
    for(i=scope.len-1; i>=0; i++)
        if(strcmp(expr->msg, scope.data[i]->ident))
            break;
    
    if(i<0)
    {
        error(true, expr->line, expr->col, "use of undeclared identifier '%s'\n", expr->msg);
        exit(1);
    }

    return scope.data[i]->type;
}

static type_e semantics_getliteraltype(uint64_t lit)
{
    uint64_t val;

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
    case TYPE_FUNC:
        if(!semantics_typecmp(a->func.ret, b->func.ret))
            return false;
        if(a->func.args.len != b->func.args.len)
            return false;
        for(i=0; i<a->func.args.len; i++)
            if(!semantics_typecmp(&a->func.args.data[i], &b->func.args.data[i]))
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

    newexpr = malloc(sizeof(expr_t));
    *newexpr = *expr;

    expr->op = EXPROP_CAST;
    expr->casttype.type = type;
    expr->operand = newexpr;
    expr->type.type = type;
}

// list should be uninitialized
static void semantics_getoperands(list_pexpr_t* list, expr_t* expr)
{
    int i;

    int nop;

    assert(expr);

    nop = exprop_nop[expr->op];
    if(nop == -1)
        list_pexpr_dup(list, &expr->variadic);
    else
    {
        list_pexpr_init(list, nop);
        for(i=0; i<nop; i++)
            list->data[i] = expr->operands[i];
    }
}

static void semantics_expr(expr_t* expr);

static void semantics_binaryexpr(expr_t* expr)
{
    int i;

    for(i=0; i<2; i++)
    {
        semantics_expr(expr->operands[i]);
        if(expr->operands[i]->type.type < TYPE_I32)
            semantics_cast(TYPE_I32, expr->operands[i]);
    }

    expr->type.type = TYPE_I32;
    for(i=0; i<2; i++)
        if(expr->operands[i]->type.type > expr->type.type)
            expr->type.type = expr->operands[i]->type.type;
}

static void semantics_condition(expr_t* expr)
{
    semantics_expr(expr);

    // TODO: in the future, check for integeral type
    // i havent implemented non-integral types yet
    switch(expr->type.type)
    {
    default:
        break;
    }
}

static void semantics_ternaryexpr(expr_t* expr)
{
    int i;

    semantics_condition(expr->operands[0]);

    for(i=1; i<3; i++)
    {
        semantics_expr(expr->operands[i]);
        if(expr->operands[i]->type.type < TYPE_I32)
            semantics_cast(TYPE_I32, expr->operands[i]);
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

    if(!expr->operands[i]->lval)
        error(true, expr->line, expr->col, "expression is not assignable");

    expr->type = expr->operands[0]->type;
    if(expr->type.type != expr->operands[1]->type.type)
        semantics_cast(expr->type, expr->operands[1]);
}

static void semantics_unaryexpr(expr_t* expr)
{
    type_t inttype;

    inttype.type = TYPE_I32;

    semantics_expr(expr->operand);
    if(expr->operand->type.type < TYPE_I32)
        semantics_cast(inttype, expr->operand);
    expr->type = expr->operand->type;
}

// unary no-op, e.g. unary +
static void semantics_noop(expr_t* expr)
{
    expr_t* child;

    semantics_expr(expr->operand);

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

    functype = &expr->variadic.data[0]->type;
    if(functype->type != TYPE_FUNC)
        error(true, expr->line, expr->col, "expected function or function pointer\n");

    if(expr->variadic.len - 1 < functype->func.args.len)
        error(true, expr->line, expr->col, "too few arguments to function call\n");
    if(expr->variadic.len - 1 > functype->func.args.len)
        error(true, expr->line, expr->col, "too many arguments to function call\n");

    for(i=0; i<functype->func.args.len; i++)
    {
        semantics_expr(expr->variadic.data[i+1]);
        if(!semantics_typecmp(&expr->variadic.data[i+1]->type, &functype->func.args.data[i]))
            continue;
        semantics_cast(expr->variadic.data[i+1], expr->variadic.data[i+1]);
    }
}

static void semantics_expr(expr_t* expr)
{
    int i;

    expr->lval = false;

    switch(expr->op)
    {
    case EXPROP_LIT:
        expr->type.type = semantics_getliteraltype(expr->u64);
        break;
    case EXPROP_VAR:
        expr->type = semantics_getvariabletype(expr);
        expr->lval = true;
        break;
    case EXPROP_COND:
        semantics_ternaryexpr(expr);
    case EXPROP_ASSIGN:
        semantics_assignexpr(expr);
        break;
    case EXPROP_ADD:
    case EXPROP_SUB:
    case EXPROP_MULT:
    case EXPROP_DIV:
        semantics_binaryexpr(expr);
        break;
    case EXPROP_NEG:
    case EXPROP_PREINC:
    case EXPROP_PREDEC:
    case EXPROP_POSTINC:
    case EXPROP_POSTDEC:
        semantics_unaryexpr(expr);
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
    default:
        assert(0);
        break;
    }
}

static void semantics_return(globaldecl_t* func, stmnt_t* stmnt)
{
    semantics_expr(stmnt->expr);
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
        semanitcs_condition(stmnt->ifstmnt.expr);
        semanitcs_statement(stmnt->ifstmnt.ifblk);
        break;
    case STMNT_WHILE:
        semanitcs_condition(stmnt->whilestmnt.expr);
        semanitcs_statement(stmnt->whilestmnt.body);
        break;
    default:
        break;
    }
}

static void semantics_compound(globaldecl_t* func, compound_t* compound)
{
    int i;

    uint64_t scopesize;

    scopesize = scope.len;

    for(i=0; i<compound->decls.len; i++)
        list_pdecl_push(&scope, &compound->decls.data[i]);

    for(i=0; i<compound->stmnts.len; i++)
        semantics_statement(func, &compound->stmnts.data[i]);

    list_pdecl_resize(&scope, scopesize);
}

static void semantics_funcdef(globaldecl_t* func)
{
    list_pdecl_init(&scope, 0);
    semantics_compound(func, &func->funcdef);
    list_pdecl_free(&scope);
}

void semantics(void)
{
    int i;

    for(i=0; i<ast.len; i++)
    {
        if(!ast.data[i].hasfuncdef)
            continue;
        semantics_funcdef(&ast.data[i]);
    }
}