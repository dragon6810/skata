#include "ast.h"

#include <stdio.h>

#include "token.h"
#include "type.h"

token_t *curtok;

static void parse_freetypedvar(typedvar_t* var)
{
    free(var->ident);
}

static void parse_freeglobaldecl(globaldecl_t* decl)
{
    switch(decl->form)
    {
    case DECL_VAR:
        free(decl->decl.ident);
        list_typedvar_free(&decl->decl.args);
        break;
    default:
        break;
    }
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

static void parse_eatform(token_e form)
{
    const char *formname;

    if(parse_peekform(0) != form)
    {
        switch(form)
        {
        case TOKEN_EOF:
            formname = "eof";
            break;
        case TOKEN_IDENT:
        case TOKEN_RID:
            formname = "identifier";
            break;
        case TOKEN_NUMBER:
            formname = "number";
            break;
        case TOKEN_STRING:
            formname = "string";
            break;
        case TOKEN_PUNC:
            formname = "puncuation";
            break;
        case TOKEN_TYPE:
            formname = "type";
            break;
        }
        printf("expected %s\n", formname);
        exit(1);
    }

    curtok++;
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

static void parse_globaldecl(void)
{
    parse_eatform(TOKEN_TYPE);
    parse_eatform(TOKEN_IDENT);
    parse_eatstr(";");
}

void parse(void)
{
    curtok = tokens.data;

    while(curtok->form != TOKEN_EOF)
        parse_globaldecl();
}
