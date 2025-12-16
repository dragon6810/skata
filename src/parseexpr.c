#include "ast.h"

#include <inttypes.h>
#include <stdio.h>

#include "token.h"

static void parse_printexpr_r(const expr_t* expr)
{
    int i;

    const char *op;
    int nterms;
    bool postfix;

    if(!expr)
        return;

    nterms = 0;
    postfix = false;
    switch(expr->op)
    {
    case EXPROP_LIT:
        switch(expr->type.type)
        {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
            printf("%" PRIi64, expr->i64);
            break;
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
            printf("%" PRIu64, expr->u64);
            break;
        default:
            assert(0);
            break;
        }
        return;
    case EXPROP_VAR:
        printf("%s", expr->msg);
        return;
    case EXPROP_COND:
        nterms = 3;
        op = "?:";
        break;
    case EXPROP_ASSIGN:
        nterms = 2;
        op = "=";
        break;
    case EXPROP_ADD:
        nterms = 2;
        op = "+";
        break;
    case EXPROP_SUB:
        nterms = 2;
        op = "-";
        break;
    case EXPROP_MULT:
        nterms = 2;
        op = "*";
        break;
    case EXPROP_DIV:
        nterms = 2;
        op = "/";
        break;
    case EXPROP_NEG:
        nterms = 1;
        op = "-";
        break;
    case EXPROP_POS:
        nterms = 1;
        op = "+";
        break;
    case EXPROP_PREINC:
        nterms = 1;
        op = "++";
        break;
    case EXPROP_PREDEC:
        nterms = 1;
        op = "--";
        break;
    case EXPROP_POSTINC:
        nterms = 1;
        op = "++";
        postfix = true;
        break;
    case EXPROP_POSTDEC:
        nterms = 1;
        op = "--";
        postfix = true;
        break;
    case EXPROP_CALL:
        printf("call ");
        parse_printexpr_r(expr->operand);
        printf(" ( ");
        for(i=0; i<expr->variadic.len; i++)
        {
            parse_printexpr_r(expr->variadic.data[i]);
            printf(" ");
        }
        printf(")");
        return;
    case EXPROP_CAST:
        printf("( cast (%s) ", type_names[expr->casttype.type]);
        parse_printexpr_r(expr->operand);
        printf(" )");
        return;
    default:
        assert(0);
        return;
    }

    if(!postfix)
        printf("( %s ", op);
    else
        printf("( ");

    for(i=0; i<nterms; i++)
    {
        parse_printexpr_r(expr->operands[i]);
        printf(" ");
    }

    if(!postfix)
        printf(")");
    else
        printf("%s )", op);
}

void parse_printexpr(const expr_t* expr)
{
    parse_printexpr_r(expr);
}

static int parse_postfixopbp(exprop_e op)
{
    switch(op)
    {
    case EXPROP_POSTINC:
    case EXPROP_POSTDEC:
        return 11;
    default:
        return 0;
    }
}

static int parse_prefixopbp(exprop_e op)
{
    switch(op)
    {
    case EXPROP_NEG:
    case EXPROP_POS:
    case EXPROP_PREINC:
    case EXPROP_PREDEC:
        return 10;
    case EXPROP_CAST:
        return 9;
    default:
        return 0;
    }
}

static void parse_ternaryopbp(exprop_e op, int bp[2])
{
    switch(op)
    {
    case EXPROP_COND:
        bp[0] = 4;
        bp[1] = 3;
    default:
        bp[0] = bp[1] = 0;
        break;
    }
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
        bp[0] = 5;
        bp[1] = 6;
        break;
    case EXPROP_MULT:
    case EXPROP_DIV:
        bp[0] = 7;
        bp[1] = 8;
        break;
    default:
        bp[0] = bp[1] = 0;
        break;
    }
}

static expr_t* parse_number(void)
{
    expr_t* expr;

    expr = malloc(sizeof(expr_t));
    expr->line = parse_getline();
    expr->col = parse_getcol();
    expr->op = EXPROP_LIT;
    sscanf(parse_eat(), "%" PRIu64, &expr->u64);

    return expr;
}   

static expr_t* parse_expr_r(int minbp)
{
    expr_t *expr, *lhs, *rhs, *thenexpr, *elseexpr;
    const char *tokstr;
    exprop_e op;
    int bp[2];

    expr = NULL;

    // atom/prefix
    switch(parse_peekform(0))
    {
    case TOKEN_NUMBER:
        expr = parse_number();
        break;
    case TOKEN_IDENT:
        expr = malloc(sizeof(expr_t));
        expr->line = parse_getline();
        expr->col = parse_getcol();
        expr->op = EXPROP_VAR;
        expr->msg = strdup(parse_eat());
        break;
    case TOKEN_PUNC:
        if(!strcmp(parse_peekstr(0), "-"))
            op = EXPROP_NEG;
        else if(!strcmp(parse_peekstr(0), "+"))
            op = EXPROP_POS;
        else if(!strcmp(parse_peekstr(0), "++"))
            op = EXPROP_PREINC;
        else if(!strcmp(parse_peekstr(0), "--"))
            op = EXPROP_PREDEC;
        else if(!strcmp(parse_peekstr(0), "("))
        {
            parse_eat();

            if(parse_istype())
            {
                expr = malloc(sizeof(expr_t));
                expr->line = parse_getline();
                expr->col = parse_getcol();
                expr->op = EXPROP_CAST;
                parse_type(&expr->casttype);
                if(!parse_eatstr(")"))
                    exit(0);
                expr->operand = parse_expr_r(parse_prefixopbp(EXPROP_CAST));
                break;
            }
            expr = parse_expr_r(0);
            parse_eatstr(")");
            break;
        }
        else
            return expr;

        parse_eat();

        expr = malloc(sizeof(expr_t));
        expr->line = parse_getline();
        expr->col = parse_getcol();
        expr->op = op;
        expr->operand = parse_expr_r(parse_prefixopbp(op));

        break;
    default:
        return expr;
    }

    // postfix/ternary/infix
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
        else if(!strcmp(tokstr, "++"))
            op = EXPROP_POSTINC;
        else if(!strcmp(tokstr, "--"))
            op = EXPROP_POSTDEC;
        else if(!strcmp(tokstr, "("))
        {
            parse_eatstr("(");

            lhs = expr;

            expr = malloc(sizeof(expr_t));
            expr->line = parse_getline();
            expr->col = parse_getcol();
            expr->op = EXPROP_CALL;
            expr->operand = lhs;
            list_pexpr_init(&expr->variadic, 0);

            while(strcmp(parse_peekstr(0), ")"))
            {
                list_pexpr_push(&expr->variadic, parse_expr_r(0));
                if(strcmp(parse_peekstr(0), ")"))
                    parse_eatstr(",");
            }

            parse_eatstr(")");

            goto continueloop;
        }
        else if(!strcmp(tokstr, "?"))
        {
            op = EXPROP_COND;
            parse_ternaryopbp(op, bp);

            if(bp[0] < minbp)
                goto done;

            parse_eat();
            thenexpr = parse_expr_r(bp[1]);
            if(!thenexpr)
            {
                printf("expected expression.\n");
                exit(1);
            }
            parse_eatstr(":");
            elseexpr = parse_expr_r(bp[1]);
            if(!elseexpr)
            {
                printf("expected expression.\n");
                exit(1);
            }

            lhs = expr;
            expr = malloc(sizeof(expr_t));
            expr->line = parse_getline();
            expr->col = parse_getcol();
            expr->op = EXPROP_COND;
            expr->operands[0] = lhs;
            expr->operands[1] = thenexpr;
            expr->operands[2] = elseexpr;

            goto continueloop;
        }
        else
            break;

        switch(op)
        {
        case EXPROP_POSTINC:
        case EXPROP_POSTDEC:
            bp[0] = parse_postfixopbp(op);
            if(bp[0] < minbp)
                goto done;

            parse_eat();

            lhs = expr;

            expr = malloc(sizeof(expr_t));
            expr->line = parse_getline();
            expr->col = parse_getcol();
            expr->op = op;
            expr->operand = lhs;

            goto continueloop;
        default:
            break;
        }

        parse_infixopbp(op, bp);
        if(bp[0] < minbp)
            goto done;

        parse_eat();
        
        rhs = parse_expr_r(bp[1]);
        if(!rhs)
            goto done;

        lhs = expr;

        expr = malloc(sizeof(expr_t));
        expr->line = parse_getline();
        expr->col = parse_getcol();
        expr->op = op;
        expr->operands[0] = lhs;
        expr->operands[1] = rhs;

continueloop:
        continue;
    }

done:
    return expr;
}

expr_t* parse_expr(void)
{
    return parse_expr_r(0);
}