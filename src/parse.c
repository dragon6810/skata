#include "ast.h"

#include <stdio.h>

#include "token.h"
#include "type.h"

list_globaldecl_t ast;

token_t *curtok;

static void parse_freetypedvar(typedvar_t* var)
{
    free(var->ident);
}

static void parse_freeglobaldecl(globaldecl_t* decl)
{
    free(decl->decl.ident);
    list_typedvar_free(&decl->decl.args);
}

LIST_DEF(typedvar)
LIST_DEF_FREE_DECONSTRUCT(typedvar, parse_freetypedvar)

LIST_DEF(globaldecl)
LIST_DEF_FREE_DECONSTRUCT(globaldecl, parse_freeglobaldecl)

static token_e parse_peekform(int offs)
{
    token_t *tok;

    tok = curtok + offs;
    if(tok->form == TOKEN_IDENT && type_find(tok->msg) >= 0)
        return TOKEN_TYPE;
    if(tok->form == TOKEN_RID && type_find(rid_strs[tok->rid]) >= 0)
        return TOKEN_TYPE;
    return tok->form;
}

static const char* parse_peekstr(int offs)
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

static const char* parse_eatform(token_e form)
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

static void parse_eatstr(const char* str)
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

static void parse_arglist(decl_t* decl)
{
    typedvar_t arg;

    while(1)
    {
        if(!strcmp(parse_peekstr(0), ")"))
            break;

        arg.type = type_find(parse_eatform(TOKEN_TYPE));
        arg.ident = strdup(parse_eatform(TOKEN_IDENT));
        list_typedvar_ppush(&decl->args, &arg);

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
    list_typedvar_init(&decl.decl.args, 0);

    if(!strcmp(parse_peekstr(0), "("))
    {   
        decl.decl.form = DECL_FUNC;

        parse_eatstr("(");

        parse_arglist(&decl.decl);
        
        parse_eatstr(")");
    }

    parse_eatstr(";");
    
    list_globaldecl_ppush(&ast, &decl);
}

void parse(void)
{
    int i;

    curtok = tokens.data;

    while(curtok->form != TOKEN_EOF)
        parse_globaldecl();

    for(i=0; i<ast.len; i++)
        printf("type: %d, name: %s.\n", ast.data[i].decl.type, ast.data[i].decl.ident);
}
