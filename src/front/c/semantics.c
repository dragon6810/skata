#include <assert.h>
#include <stdio.h>

#include "ast.h"
#include "front/error.h"

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

static void semantics_typecpy(type_t* dst, type_t* src)
{
    *dst = *src;

    switch(dst->type)
    {
    case TYPE_FUNC:
        if(dst->func.ret)
        {
            dst->func.ret = malloc(sizeof(type_t));
            semantics_typecpy(dst->func.ret, src->func.ret);
        }
        list_type_dup(&dst->func.args, &src->func.args);
        break;
    default:
        break;
    }
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

    if(semantics_typecmp(&expr->type, &type))
        return;

    newexpr = malloc(sizeof(expr_t));
    *newexpr = *expr;

    expr->op = EXPROP_CAST;
    expr->operand = newexpr;
    semantics_typecpy(&expr->type, &type);
    semantics_typecpy(&expr->casttype, &type);
}

static void semantics_expr(expr_t* expr);

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
        semantics_typecpy(&expr->type, &expr->operands[1]->type);
    }
    else
    {
        semantics_cast(expr->operands[0]->type, expr->operands[1]);
        semantics_typecpy(&expr->type, &expr->operands[0]->type);
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
        error(true, expr->line, expr->col, "expression is not assignable");

    expr->type = expr->operands[0]->type;
    if(!semantics_typecmp(&expr->type, &expr->operands[1]->type))
        semantics_cast(expr->type, expr->operands[1]);
}

static void semantics_logicalnot(expr_t* expr)
{
    semantics_truthify(expr->operand);
    expr->type.type = TYPE_U1;
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

    semantics_typecpy(&expr->type, functype->func.ret);
}

static void semantics_var(expr_t* expr)
{
    int i, j;

    type_t type;

    assert(expr->op == EXPROP_VAR);

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
        semantics_typecpy(expr->type.func.ret, &scope.data[i]->type);

        list_type_init(&expr->type.func.args, 0);
        for(j=0; j<scope.data[i]->args.len; j++)
        {
            semantics_typecpy(&type, &scope.data[i]->args.data[j].type);
            list_type_ppush(&expr->type.func.args, &type);
        }

        expr->lval = false;
        return;
    }

    semantics_typecpy(&expr->type, &scope.data[i]->type);
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
    case EXPROP_NEG:
    case EXPROP_PREINC:
    case EXPROP_PREDEC:
    case EXPROP_POSTINC:
    case EXPROP_POSTDEC:
        semantics_unaryexpr(expr);
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
        break;
    case STMNT_WHILE:
        semantics_condition(stmnt->whilestmnt.expr);
        semantics_statement(func, stmnt->whilestmnt.body);
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
    int i;

    uint64_t scopesize;

    list_pdecl_push(&scope, &func->decl);

    scopesize = scope.len;
    for(i=0; i<func->decl.args.len; i++)
        list_pdecl_push(&scope, &func->decl.args.data[i]);
    semantics_compound(func, &func->funcdef);
    list_pdecl_resize(&scope, scopesize);
}

void semantics(void)
{
    int i;

    list_pdecl_init(&scope, 0);

    for(i=0; i<ast.len; i++)
    {
        if(!ast.data[i].hasfuncdef)
            continue;
        semantics_funcdef(&ast.data[i]);
    }

    list_pdecl_free(&scope);
}