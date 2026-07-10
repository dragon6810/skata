#include "constexpr.h"

#include "front/error.h"

int64_t constexpr_eval(const expr_t* expr)
{
    switch(expr->op)
    {
    case EXPROP_LIT:
        return expr->i64;
    case EXPROP_COND:
        return constexpr_eval(expr->operands[0]) ? constexpr_eval(expr->operands[1]) : constexpr_eval(expr->operands[2]);
    case EXPROP_ADD:
        return constexpr_eval(expr->operands[0]) + constexpr_eval(expr->operands[1]);
    case EXPROP_SUB:
        return constexpr_eval(expr->operands[0]) - constexpr_eval(expr->operands[1]);
    case EXPROP_MULT:
        return constexpr_eval(expr->operands[0]) * constexpr_eval(expr->operands[1]);
    case EXPROP_DIV:
        return constexpr_eval(expr->operands[0]) / constexpr_eval(expr->operands[1]);
    case EXPROP_EQ:
        return constexpr_eval(expr->operands[0]) == constexpr_eval(expr->operands[1]);
    case EXPROP_NEQ:
        return constexpr_eval(expr->operands[0]) != constexpr_eval(expr->operands[1]);
    case EXPROP_NEG:
        return -constexpr_eval(expr->operand);
    case EXPROP_POS:
        return +constexpr_eval(expr->operand);
    case EXPROP_LOGICNOT:
        return !constexpr_eval(expr->operand);
    default:
        error(true, expr->line, expr->col, "illegal operator for constant expression\n");
        return 0;
    }
}