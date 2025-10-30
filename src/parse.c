#include "ast.h"

#include <stdio.h>

#include "type.h"

list_globaldecl_t ast;

token_t *curtok;

static void parse_freedecl(decl_t* decl)
{
    free(decl->ident);
    list_decl_free(&decl->args);
}

static void parse_freeglobaldecl(globaldecl_t* decl)
{
    parse_freedecl(&decl->decl);
    if(decl->hasfuncdef)
        list_decl_free(&decl->funcdef.decls);
}

static void parse_freeexpr(expr_t* expr)
{
    switch(expr->op)
    {
    case EXPROP_ATOM:
        free(expr->msg);
        break;
    case EXPROP_ADD:
    case EXPROP_MULT:
        parse_freeexpr(expr->operands[0]);
        parse_freeexpr(expr->operands[1]);
        break;
    default:
        break;
    }

    free(expr);
}

static void parse_freestmnt(stmnt_t* stmnt)
{
    switch(stmnt->form)
    {
    case STMNT_EXPR:
        parse_freeexpr(stmnt->expr);
        break;
    case STMNT_COMPOUND:
        list_decl_free(&stmnt->compound.decls);
        list_stmnt_free(&stmnt->compound.stmnts);
        break;
    default:
        break;
    }
}

LIST_DEF(decl)
LIST_DEF_FREE_DECONSTRUCT(decl, parse_freedecl)

LIST_DEF(globaldecl)
LIST_DEF_FREE_DECONSTRUCT(globaldecl, parse_freeglobaldecl)

LIST_DEF(stmnt)
LIST_DEF_FREE_DECONSTRUCT(stmnt, parse_freestmnt)

token_e parse_peekform(int offs)
{
    token_t *tok;

    tok = curtok + offs;
    if(tok->form == TOKEN_IDENT && type_find(tok->msg) >= 0)
        return TOKEN_TYPE;
    if(tok->form == TOKEN_RID && type_find(rid_strs[tok->rid]) >= 0)
        return TOKEN_TYPE;
    return tok->form;
}

const char* parse_peekstr(int offs)
{
    token_t *tok;

    tok = curtok + offs;
    switch(tok->form)
    {
    case TOKEN_IDENT:
        return tok->msg;
    case TOKEN_RID:
        return rid_strs[tok->rid];
    case TOKEN_NUMBER:
        return tok->msg;
    case TOKEN_STRING:
        return tok->msg;
    case TOKEN_PUNC:
        return punc_strs[tok->punc];
    default:
        return "";
    }
}

const char* parse_eatform(token_e form)
{
    const char *str;

    if(parse_peekform(0) != form)
    {
        switch(form)
        {
        case TOKEN_EOF:
            str = "eof";
            break;
        case TOKEN_IDENT:
        case TOKEN_RID:
            str = "identifier";
            break;
        case TOKEN_NUMBER:
            str = "number";
            break;
        case TOKEN_STRING:
            str = "string";
            break;
        case TOKEN_PUNC:
            str = "puncuation";
            break;
        case TOKEN_TYPE:
            str = "type";
            break;
        }
        printf("expected %s\n", str);
        exit(1);
    }

    curtok++;
    return parse_peekstr(-1);
}

void parse_eatstr(const char* str)
{
    const char *tokstr;

    tokstr = parse_peekstr(0);
    if(strcmp(tokstr, str))
    {
        printf("expected '%s'\n", str);
        exit(1);
    }

    curtok++;
}

const char* parse_eat(void)
{
    token_t *oldtok;

    oldtok = curtok++;
    switch(oldtok->form)
    {
    case TOKEN_IDENT:
    case TOKEN_NUMBER:
    case TOKEN_STRING:
        return oldtok->msg;
    case TOKEN_PUNC:
        return punc_strs[oldtok->punc];
    case TOKEN_RID:
        return rid_strs[oldtok->rid];
    default:
        return "";
    }
}

static void parse_decl(decl_t* decl)
{
    decl->form = DECL_VAR;
    decl->type = type_find(parse_eatform(TOKEN_TYPE));
    decl->ident = strdup(parse_eatform(TOKEN_IDENT));
    decl->expr = NULL;

    if(!strcmp(parse_peekstr(0), "="))
    {
        curtok--;
        decl->expr = parse_expr();
    }

    parse_eatstr(";");
}

static void parse_statement(stmnt_t* stmnt)
{
    if(!strcmp(parse_peekstr(0), "return"))
    {
        parse_eat();

        stmnt->form = STMNT_RETURN;
        stmnt->expr = parse_expr();
        parse_eatstr(";");
        return;
    }

    stmnt->form = STMNT_EXPR;
    stmnt->expr = parse_expr();
    parse_eatstr(";");

    return;
}

static void parse_compound(compound_t* cmpnd)
{
    decl_t decl;
    stmnt_t stmnt;

    list_decl_init(&cmpnd->decls, 0);
    list_stmnt_init(&cmpnd->stmnts, 0);

    parse_eatstr("{");

    while(strcmp(parse_peekstr(0), "}"))
    {
        if(parse_peekform(0) == TOKEN_TYPE)
        {
            parse_decl(&decl);
            list_decl_ppush(&cmpnd->decls, &decl);
        }
        else
        {
            parse_statement(&stmnt);
            list_stmnt_ppush(&cmpnd->stmnts, &stmnt);
        }
    }

    parse_eatstr("}");
}

static void parse_arglist(decl_t* decl)
{
    decl_t arg;

    while(1)
    {
        if(!strcmp(parse_peekstr(0), ")"))
            break;

        arg.type = type_find(parse_eatform(TOKEN_TYPE));
        arg.ident = strdup(parse_eatform(TOKEN_IDENT));
        list_decl_init(&arg.args, 0);

        list_decl_ppush(&decl->args, &arg);

        if(strcmp(parse_peekstr(0), ")"))
            parse_eatstr(",");
    }
}

static void parse_globaldecl(void)
{
    globaldecl_t decl;

    decl.hasfuncdef = false;
    decl.decl.form = DECL_VAR;
    decl.decl.type = type_find(parse_eatform(TOKEN_TYPE));
    decl.decl.ident = strdup(parse_eatform(TOKEN_IDENT));
    decl.decl.expr = NULL;
    list_decl_init(&decl.decl.args, 0);

    if(!strcmp(parse_peekstr(0), "("))
    {   
        decl.decl.form = DECL_FUNC;

        parse_eatstr("(");
        parse_arglist(&decl.decl);
        parse_eatstr(")");
    }

    if(decl.decl.form == DECL_FUNC && !strcmp(parse_peekstr(0), "{"))
    {   
        decl.hasfuncdef = true;
        parse_compound(&decl.funcdef);
    }
    else
        parse_eatstr(";");
    
    list_globaldecl_ppush(&ast, &decl);
}

static char* parse_printprefix(int depth, bool last, char* prefix)
{
    char *newprefix;

    printf("\033[1;35m%s%s\033[0m", prefix, (depth ? (last ? "`-" : "|-") : "-"));

    newprefix = malloc(strlen(prefix) + 3);
    snprintf(newprefix, strlen(prefix) + 3, "%s%s ", prefix, (last ? " " : "|"));

    return newprefix;
}

static void parse_printexprast(expr_t* expr, int depth, bool last, char* leftstr)
{
    char* newleft;

    newleft = parse_printprefix(depth, last, leftstr);

    printf("\e[1;32m<expression> \e[0;96m");
    parse_printexpr(expr);
    printf("\e[0m");

    free(newleft);
}

static void parse_printexprstatement(stmnt_t* stmnt, int depth, bool last, char* leftstr)
{
    char* newleft;

    newleft = parse_printprefix(depth, last, leftstr);

    printf("\e[1;32m<expression-statement>\e[0m\n");
    if(stmnt->expr)
        parse_printexprast(stmnt->expr, depth + 1, true, newleft);

    free(newleft);
}

static void parse_printreturnstatement(stmnt_t* stmnt, int depth, bool last, char* leftstr)
{
    char* newleft;

    newleft = parse_printprefix(depth, last, leftstr);

    printf("\e[1;32m<return-statement>\e[0m\n");
    if(stmnt->expr)
        parse_printexprast(stmnt->expr, depth + 1, true, newleft);

    free(newleft);
}

static void parse_printcompound(compound_t* def, int depth, bool last, char* leftstr);

static void parse_printstatement(stmnt_t* stmnt, int depth, bool last, char* leftstr)
{
    switch(stmnt->form)
    {
    case STMNT_COMPOUND:
        parse_printcompound(&stmnt->compound, depth, last, leftstr);
        break;
    case STMNT_EXPR:
        parse_printexprstatement(stmnt, depth, last, leftstr);
        break;
    case STMNT_RETURN:
        parse_printreturnstatement(stmnt, depth, last, leftstr);
        break;
    }
}

static void parse_printdecl(decl_t* decl, int depth, bool last, char* leftstr)
{
    char* newleft;

    newleft = parse_printprefix(depth, last, leftstr);

    printf("\e[1;32m<declaration> \e[0;96m(type: %s), (name: %s)\e[0m\n", types.data[decl->type], decl->ident);
    if(decl->expr)
        parse_printexprast(decl->expr, depth + 1, true, newleft);

    free(newleft);
}

static void parse_printcompound(compound_t* def, int depth, bool last, char* leftstr)
{
    int i;

    char *newleft;

    newleft = parse_printprefix(depth, last, leftstr);

    printf("\e[1;32m<compound-statement>\e[0m\n");

    for(i=0; i<def->decls.len; i++)
        parse_printdecl(&def->decls.data[i], depth + 1, i == def->decls.len - 1 && !def->stmnts.len, newleft);

    for(i=0; i<def->stmnts.len; i++)
        parse_printstatement(&def->stmnts.data[i], depth + 1, i == def->stmnts.len - 1, newleft);

    free(newleft);
}

static void parse_printglobaldecl(globaldecl_t* decl, int depth, bool last, char* leftstr)
{
    char *newleft;

    newleft = parse_printprefix(depth, last, leftstr);

    printf("\e[1;32m<external-decl>\e\n");
    parse_printdecl(&decl->decl, depth + 1, !decl->hasfuncdef, newleft);
    if(decl->hasfuncdef)
        parse_printcompound(&decl->funcdef, depth + 1, true, newleft); 

    free(newleft);  
}

void parse(void)
{
    int i;

    curtok = tokens.data;

    while(curtok->form != TOKEN_EOF)
        parse_globaldecl();

    for(i=0; i<ast.len; i++)
        parse_printglobaldecl(&ast.data[i], 1, i == ast.len - 1, "");
}
