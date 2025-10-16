#include "token.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* srctext = NULL;
token_t* tokens = NULL;

token_t* tokenstail = NULL;
char* curpos = NULL;
int line = 0, column = 0;

void lex_pushtok(token_t* tok)
{
    if(tokenstail)
    {
        tokenstail->next = tok;
        tokenstail = tok;
    }
    else
        tokenstail = tokens = tok;
}

bool lex_trynumber(void)
{
    char* c;
    int len;
    token_t *tok;

    c = curpos;
    while(*c >= '0' && *c <= '9')
        c++;

    if(c == curpos)
        return false;

    len = c - curpos;

    tok = malloc(sizeof(token_t));
    tok->line = line;
    tok->column = column;
    tok->form = TOKEN_NUMBER;
    tok->msg = malloc(len + 1);
    memcpy(tok->msg, curpos, len);
    tok->msg[len] = 0;
    tok->next = NULL;
    lex_pushtok(tok);

    curpos += len;
    column += len;

    return true;
}

bool lex_tryident(void)
{
    rid_e rid;

    char* c;
    int len;
    token_t *tok;

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

        tok = malloc(sizeof(token_t));
        tok->line = line;
        tok->column = column;
        tok->form = TOKEN_RID;
        tok->rid = rid;
        tok->next = NULL;
        lex_pushtok(tok);

        curpos += len;
        column += len;

        return true;
    }

    len = c - curpos;

    tok = malloc(sizeof(token_t));
    tok->line = line;
    tok->column = column;
    tok->form = TOKEN_IDENT;
    tok->msg = malloc(len + 1);
    memcpy(tok->msg, curpos, len);
    tok->msg[len] = 0;
    tok->next = NULL;
    lex_pushtok(tok);

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
    token_t *tok;

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

    tok = malloc(sizeof(token_t));
    tok->line = line;
    tok->column = column;
    tok->form = TOKEN_PUNC;
    tok->punc = besttype;
    tok->next = NULL;
    lex_pushtok(tok);

    curpos += bestlen;
    column += bestlen;

    return true;
}

bool lex_tryeof(void)
{
    token_t *tok;

    if(*curpos)
        return false;

    tok = malloc(sizeof(token_t));
    tok->line = line;
    tok->column = column;
    tok->form = TOKEN_EOF;
    tok->next = NULL;
    lex_pushtok(tok);

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
    token_t *curtok;

    curpos = srctext;
    line = column = 0;

    do
        lex_nexttok();
    while(tokenstail->form != TOKEN_EOF);

    for(curtok=tokens; curtok; curtok=curtok->next)
    {
        switch(curtok->form)
        {
        case TOKEN_EOF:
            printf("EOF");
            break;
        case TOKEN_IDENT:
        case TOKEN_NUMBER:
        case TOKEN_STRING:
            printf("%s", curtok->msg);
            break;
        case TOKEN_RID:
            printf("%s", rid_strs[curtok->rid]);
            break;
        case TOKEN_PUNC:
            printf("%s", punc_strs[curtok->punc]);
            break;
        default:
            break;
        }

        printf(" (%d:%d)\n", curtok->line + 1, curtok->column + 1);
    }
}