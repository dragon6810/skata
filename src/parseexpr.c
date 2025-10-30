#include "ast.h"

#include <stdio.h>

#include "token.h"

static bool parse_tokenisatom(void)
{
    if(parse_peekform(0) == TOKEN_NUMBER)
        return true;

    if(parse_peekform(0) == TOKEN_IDENT)
        return true;

    return false;
}

static void parse_printexpr_r(const expr_t* expr)
{
    char op;

    switch(expr->op)
    {
    case EXPROP_ATOM:
        printf("%s", expr->msg);
        return;
    case EXPROP_ASSIGN:
        op = '=';
        break;
    case EXPROP_ADD:
        op = '+';
        break;
    case EXPROP_SUB:
        op = '-';
        break;
    case EXPROP_MULT:
        op = '*';
        break;
    case EXPROP_DIV:
        op = '/';
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

static void parse_infixopbp(exprop_e op, int bp[2])
{
    switch(op)
    {
    case EXPROP_ASSIGN:
        bp[0] = 2;
        bp[1] = 1;
        break;
    case EXPROP_ADD:
    case EXPROP_SUB:
        bp[0] = 3;
        bp[1] = 4;
        break;
    case EXPROP_MULT:
    case EXPROP_DIV:
        bp[0] = 5;
        bp[1] = 6;
        break;
    default:
        bp[0] = bp[1] = 0;
        break;
    }
}

static expr_t* parse_expr_r(int minbp)
{
    expr_t *expr, *lhs, *rhs;
    const char *tokstr;
    exprop_e op;
    int bp[2];

    expr = NULL;

    if(!parse_tokenisatom())
        return expr;

    expr = malloc(sizeof(expr_t));
    expr->op = EXPROP_ATOM;
    expr->msg = strdup(parse_eat());

    while(1)
    {
        tokstr = parse_peekstr(0);
        if(!strcmp(tokstr, "="))
            op = EXPROP_ASSIGN;
        else if(!strcmp(tokstr, "+"))
            op = EXPROP_ADD;
        else if(!strcmp(tokstr, "-"))
            op = EXPROP_SUB;
        else if(!strcmp(tokstr, "*"))
            op = EXPROP_MULT;
        else if(!strcmp(tokstr, "/"))
            op = EXPROP_DIV;
        else
            break;

        parse_infixopbp(op, bp);
        if(bp[0] < minbp)
            break;

        parse_eat();
        
        rhs = parse_expr_r(bp[1]);
        if(!rhs)
            break;

        lhs = expr;

        expr = malloc(sizeof(expr_t));
        expr->op = op;
        expr->terms[0] = lhs;
        expr->terms[1] = rhs;
    }

    return expr;
}

expr_t* parse_expr(void)
{
    return parse_expr_r(0);
}