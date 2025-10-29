#include "ast.h"

#include <stdio.h>

static void parse_printexpr_r(const expr_t* expr)
{
    char op;

    switch(expr->op)
    {
    case EXPROP_ATOM:
        printf("%s", expr->msg);
        return;
    case EXPROP_ADD:
        op = '+';
        break;
    case EXPROP_MULT:
        op = '*';
        break;
    default:
        return;
    }

    printf("( %c ", op);
    parse_printexpr_r(expr->terms[0]);
    printf(" ");
    parse_printexpr_r(expr->terms[1]);
    printf(" )");
}

void parse_printexpr(const expr_t* expr)
{
    parse_printexpr_r(expr);
    puts("");
}

expr_t* parse_expr(void)
{
    return NULL;
}