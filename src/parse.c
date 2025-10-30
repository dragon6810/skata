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
        parse_freeexpr(expr->terms[0]);
        parse_freeexpr(expr->terms[1]);
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
            decl.form = DECL_VAR;
            decl.type = type_find(parse_eatform(TOKEN_TYPE));
            decl.ident = strdup(parse_eatform(TOKEN_IDENT));
            parse_eatstr(";");
            
            list_decl_ppush(&cmpnd->decls, &decl);
        }
        else
        {
            stmnt.form = STMNT_EXPR;
            stmnt.expr = parse_expr();
            parse_eatstr(";");
            
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

static void parse_printcompound(compound_t* def)
{
    int i;

    printf("`- <function-definition>\n");
    
    for(i=0; i<def->decls.len; i++)
        printf(" %c- <declaration> (type: %s), (name: %s)\n", 
        i < def->decls.len - 1 ? '|' : '`', types.data[def->decls.data[i].type], def->decls.data[i].ident);

    for(i=0; i<def->stmnts.len; i++)
    {
        if(def->stmnts.data[i].form != STMNT_EXPR)
            continue;

        printf(" %c- <expression> ", 
        i < def->decls.len - 1 ? '|' : '`');
        parse_printexpr(def->stmnts.data[i].expr);
    }
}

static void parse_printglobaldecl(globaldecl_t* decl)
{
    printf("- <external-decl>\n");
    printf("%c- <declaration> (type: %s), (name: %s)\n", 
        decl->hasfuncdef ? '|' : '`', types.data[decl->decl.type], decl->decl.ident);
    if(decl->hasfuncdef)
        parse_printcompound(&decl->funcdef);   
}

void parse(void)
{
    int i;

    curtok = tokens.data;

    while(curtok->form != TOKEN_EOF)
        parse_globaldecl();

    for(i=0; i<ast.len; i++)
        parse_printglobaldecl(&ast.data[i]);
}
