#include "token.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void lex_freetok(token_t* tok)
{
    switch(tok->form)
    {
    case TOKEN_IDENT:
    case TOKEN_NUMBER:
    case TOKEN_STRING:
        free(tok->msg);
        break;
    default:
        break;
    }
}

LIST_DEF(token)
LIST_DEF_FREE_DECONSTRUCT(token, lex_freetok)

char* srctext = NULL;
list_token_t tokens;

char* curpos = NULL;
int line = 0, column = 0;

bool lex_trynumber(void)
{
    char* c;
    int len;
    token_t tok;

    c = curpos;
    while(*c >= '0' && *c <= '9')
        c++;

    if(c == curpos)
        return false;

    len = c - curpos;

    tok.line = line;
    tok.column = column;
    tok.form = TOKEN_NUMBER;
    tok.msg = malloc(len + 1);
    memcpy(tok.msg, curpos, len);
    tok.msg[len] = 0;
    list_token_ppush(&tokens, &tok);

    curpos += len;
    column += len;

    return true;
}

bool lex_tryident(void)
{
    rid_e rid;

    char* c;
    int len;
    token_t tok;

    c = curpos;
    while
    (
        (*c >= 'a' && *c <= 'z')
        || (*c >= 'a' && *c <= 'z')
        || (c != curpos && *c >= '0' && *c <= '9')
        || *c == '_'
        || *c == '$'
    )
        c++;

    if(c == curpos)
        return false;

    for(rid=0; rid<RID_COUNT; rid++)
    {
        len = strlen(rid_strs[rid]);
        if(strncmp(curpos, rid_strs[rid], len))
            continue;

        tok.line = line;
        tok.column = column;
        tok.form = TOKEN_RID;
        tok.rid = rid;
        list_token_ppush(&tokens, &tok);

        curpos += len;
        column += len;

        return true;
    }

    len = c - curpos;

    tok.line = line;
    tok.column = column;
    tok.form = TOKEN_IDENT;
    tok.msg = malloc(len + 1);
    memcpy(tok.msg, curpos, len);
    tok.msg[len] = 0;
    list_token_ppush(&tokens, &tok);

    curpos += len;
    column += len;

    return true;
}

int lex_matchpunc(punc_e type)
{
    int len;

    len = strlen(punc_strs[type]);
    if(strncmp(curpos, punc_strs[type], len))
        return 0;

    return len;
}

bool lex_trypunc(void)
{
    punc_e p;

    int bestlen;
    punc_e besttype;
    int len;
    token_t tok;

    for(p=0, bestlen=0; p<PUNC_COUNT; p++)
    {
        len = lex_matchpunc(p);
        if(len > bestlen)
        {
            bestlen = len;
            besttype = p;
        }
    }

    if(!bestlen)
        return false;

    tok.line = line;
    tok.column = column;
    tok.form = TOKEN_PUNC;
    tok.punc = besttype;
    list_token_ppush(&tokens, &tok);

    curpos += bestlen;
    column += bestlen;

    return true;
}

bool lex_tryeof(void)
{
    token_t tok;

    if(*curpos)
        return false;

    tok.line = line;
    tok.column = column;
    tok.form = TOKEN_EOF;
    list_token_ppush(&tokens, &tok);

    return true;
}

void lex_skipwhitespace(void)
{
    while(*curpos && *curpos <= 32)
    {
        column++;
        if(*curpos == '\n')
        {
            line++;
            column = 0;
        }

        curpos++;
    }
}

void lex_nexttok(void)
{
    lex_skipwhitespace();

    if(lex_tryeof())
        return;

    if(lex_trypunc())
        return;

    if(lex_tryident())
        return;

    if(lex_trynumber())
        return;

    printf("bad input.\n");
    exit(1);
}

void lex(void)
{
    list_token_init(&tokens, 0);

    curpos = srctext;
    line = column = 0;

    do
        lex_nexttok();
    while(tokens.data[tokens.len-1].form != TOKEN_EOF);
}