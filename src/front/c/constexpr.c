#include "constexpr.h"

#include <stdio.h>

#include "front/error.h"

// TODO: switch to a general type arithmetic constexpr evaluater, and the literal type returned can be checked
// for integrality (is that a word?) when need be.
int64_t intconstexpr_eval(const expr_t* expr)
{
    switch(expr->op)
    {
    case EXPROP_LIT:
        return expr->i64;
    case EXPROP_COND:
        return intconstexpr_eval(expr->operands[0]) ? intconstexpr_eval(expr->operands[1]) : intconstexpr_eval(expr->operands[2]);
    case EXPROP_ADD:
        return intconstexpr_eval(expr->operands[0]) + intconstexpr_eval(expr->operands[1]);
    case EXPROP_SUB:
        return intconstexpr_eval(expr->operands[0]) - intconstexpr_eval(expr->operands[1]);
    case EXPROP_MULT:
        return intconstexpr_eval(expr->operands[0]) * intconstexpr_eval(expr->operands[1]);
    case EXPROP_DIV:
        return intconstexpr_eval(expr->operands[0]) / intconstexpr_eval(expr->operands[1]);
    case EXPROP_EQ:
        return intconstexpr_eval(expr->operands[0]) == intconstexpr_eval(expr->operands[1]);
    case EXPROP_NEQ:
        return intconstexpr_eval(expr->operands[0]) != intconstexpr_eval(expr->operands[1]);
    case EXPROP_NEG:
        return -intconstexpr_eval(expr->operand);
    case EXPROP_POS:
        return +intconstexpr_eval(expr->operand);
    case EXPROP_LOGICNOT:
        return !intconstexpr_eval(expr->operand);
    default:
        error(true, expr->line, expr->col, "illegal operator for constant expression\n");
        return 0;
    }
}

expr_t* constexpr_eval(const expr_t* expr)
{
    expr_t *constexpr;

    if(expr->op == EXPROP_STRING)
    {
        constexpr = calloc(1, sizeof(expr_t));
        type_cpy(&constexpr->type, &expr->type);
        constexpr->op = EXPROP_STRING;
        constexpr->msg = strdup(expr->msg);
        return constexpr;
    }

    // once i switch the intconstexpr to arithconstexpr expand to all scalar types
    if(expr->type.type == TYPE_I8  || expr->type.type == TYPE_U8 
    || expr->type.type == TYPE_I16 || expr->type.type == TYPE_U16
    || expr->type.type == TYPE_I32 || expr->type.type == TYPE_U32 
    || expr->type.type == TYPE_I64 || expr->type.type == TYPE_U64)
    {
        constexpr = calloc(1, sizeof(expr_t));
        type_cpy(&constexpr->type, &expr->type);
        constexpr->op = EXPROP_LIT;
        constexpr->i64 = intconstexpr_eval(expr);
        return constexpr;
    }

    printf("%d\n", (int) expr->type.type);
    error(true, expr->line, expr->col, "expression must be constant\n");
    return NULL;
}
