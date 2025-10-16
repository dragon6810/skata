#include "token.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* srctext = NULL;
token_t* tokens = NULL;

token_t* tokenstail = NULL;
char* curpos = NULL;

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

    tok->line = tok->column = 0;
    tok->form = TOKEN_PUNC;
    tok->punc = besttype;
    tok->next = NULL;

    if(tokenstail)
    {
        tokenstail->next = tok;
        tokenstail = tok;
    }
    else
        tokenstail = tokens = tok;

    curpos += bestlen;

    return true;
}

void lex_nexttok(void)
{
    token_t *tok;
    
    if(!*curpos)
    {
        tok = malloc(sizeof(token_t));

        tok->line = tok->column = 0;
        tok->form = TOKEN_EOF;
        tok->next = NULL;

        if(tokenstail)
        {
            tokenstail->next = tok;
            tokenstail = tok;
        }
        else
            tokenstail = tokens = tok;
        
        return;
    }

    if(lex_trypunc())
        return;
}

void lex(void)
{
    token_t *curtok;

    curpos = srctext;

    do
        lex_nexttok();
    while(tokenstail->form != TOKEN_EOF);

    for(curtok=tokens; curtok; curtok=curtok->next)
    {
        switch(curtok->form)
        {
        case TOKEN_EOF:
            printf("EOF\n");
            break;
        case TOKEN_IDENT:
        case TOKEN_NUMBER:
        case TOKEN_STRING:
            printf("%s\n", curtok->msg);
            break;
        case TOKEN_RID:
            printf("%s\n", rid_strs[curtok->rid]);
            break;
        case TOKEN_PUNC:
            printf("%s\n", punc_strs[curtok->punc]);
            break;
        default:
            break;
        }
    }
}