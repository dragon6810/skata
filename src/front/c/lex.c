#include "token.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "front/error.h"

typedef struct
{
    // all arrays are parallel
    int nchars;
    char *text;
    int *linenum;
    int *colnum;
} line_t;

static void lex_freeline(line_t* line)
{
    if(!line->nchars)
        return;
    line->nchars = 0;
    free(line->text);
    free(line->linenum);
    free(line->colnum);
    line->text = NULL;
    line->linenum = line->colnum = NULL;
}

LIST_DECL(line_t, line)
LIST_DEF(line)
LIST_DEF_FREE_DECONSTRUCT(line, lex_freeline)

static list_line_t lines;

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

bool lex_trystring(void)
{
    char *c;
    int len;
    token_t tok;

    c = curpos;
    if(*c != '"')
        return false;
    c++;

    len = 0;
    for(;;c++, len++)
    {
        if(*c == '"' && *(c-1) != '\\')
            break;
        if(!*c)
            error(true, line, column, "file terminates before string literal\n");
    }

    tok.line = lines.data[line].linenum[column];
    tok.column = lines.data[line].colnum[column];
    tok.form = TOKEN_STRING;
    if(!len)
    {
        tok.msg = NULL;
        list_token_ppush(&tokens, &tok);

        curpos += 2;
        column += 2;
        return true;
    }

    tok.msg = malloc(len + 1);
    memcpy(tok.msg, curpos + 1, len);
    tok.msg[len] = 0;
    list_token_ppush(&tokens, &tok);

    curpos += len + 2;
    column += len + 2;

    return true;
}

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

    tok.line = lines.data[line].linenum[column];
    tok.column = lines.data[line].colnum[column];
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

        tok.line = lines.data[line].linenum[column];
        tok.column = lines.data[line].colnum[column];
        tok.form = TOKEN_RID;
        tok.rid = rid;
        list_token_ppush(&tokens, &tok);

        curpos += len;
        column += len;

        return true;
    }

    len = c - curpos;

    tok.line = lines.data[line].linenum[column];
    tok.column = lines.data[line].colnum[column];
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

    tok.line = lines.data[line].linenum[column];
    tok.column = lines.data[line].colnum[column];
    tok.form = TOKEN_PUNC;
    tok.punc = besttype;
    list_token_ppush(&tokens, &tok);

    curpos += bestlen;
    column += bestlen;

    return true;
}

void lex_skipwhitespace(void)
{
    while(*curpos && *curpos <= 32)
    {
        column++;
        curpos++;
    }
}

// return true if end of line
bool lex_nexttok(void)
{
    lex_skipwhitespace();

    if(column >= lines.data[line].nchars)
        return true;

    if(lex_trypunc())
        return false;

    if(lex_tryident())
        return false;

    if(lex_trynumber())
        return false;

    if(lex_trystring())
        return false;

    error(true, line, column, "unrecognized token\n");
    return false;
}

static void lex_tokenizeline(void)
{
    curpos = lines.data[line].text;
    column = 0;
    while(!lex_nexttok());
}

static void lex_tokenize(void)
{
    token_t tok;

    list_token_init(&tokens, 0);

    for(line=0; line<lines.len; line++)
        lex_tokenizeline();

    tok.form = TOKEN_EOF;
    tok.line = lines.data[line-1].linenum[lines.data[line-1].nchars-1];
    tok.column = lines.data[line-1].colnum[lines.data[line-1].nchars-1]+1;
    list_token_ppush(&tokens, &tok);

    for(int i = 0; i < tokens.len; i++)
    {
        token_t *t = &tokens.data[i];
        const char *s = "?";
        if(t->form == TOKEN_RID) s = rid_strs[t->rid];
        else if(t->form == TOKEN_PUNC) s = punc_strs[t->punc];
        else if(t->form == TOKEN_IDENT || t->form == TOKEN_NUMBER || t->form == TOKEN_STRING) s = t->msg ? t->msg : "(null)";
        else if(t->form == TOKEN_EOF) s = "<EOF>";
        fprintf(stderr, "TOK[%d] form=%d '%s' @%d:%d\n", i, t->form, s, t->line, t->column);
    }
}

// merge (lineindex+1) into line
static void lex_mergeline(int lineindex)
{
    line_t *line, *nextline;
    line_t newline;

    line = &lines.data[lineindex];
    nextline = line + 1;
    
    newline.nchars = line->nchars - 1 + nextline->nchars;
    newline.text = malloc(newline.nchars + 1);
    newline.linenum = malloc(sizeof(int) * (newline.nchars + 1));
    newline.colnum = malloc(sizeof(int) * (newline.nchars + 1));

    newline.text[newline.nchars] = 0;
    newline.linenum[newline.nchars] = -1;
    newline.colnum[newline.nchars] = -1;

    memcpy(newline.text, line->text, line->nchars - 1);
    memcpy(newline.linenum, line->linenum, sizeof(int) * (line->nchars - 1));
    memcpy(newline.colnum, line->colnum, sizeof(int) * (line->nchars - 1));

    memcpy(newline.text + line->nchars - 1, nextline->text, nextline->nchars);
    memcpy(newline.linenum + line->nchars - 1, nextline->linenum, sizeof(int) * nextline->nchars);
    memcpy(newline.colnum + line->nchars - 1, nextline->colnum, sizeof(int) * nextline->nchars);

    list_line_remove(&lines, lineindex + 1);
    lex_freeline(&lines.data[lineindex]);
    lines.data[lineindex] = newline;
}

static void lex_mergelines(void)
{
    int i;

    for(i=0; i<lines.len; i++)
    {
        if(lines.data[i].text[lines.data[i].nchars-1] != '\\')
            continue;
        if(i == lines.len-1)
            error(true, 
                lines.data[i].linenum[lines.data[i].nchars-1],
                lines.data[i].colnum[lines.data[i].nchars-1],
                "can not cancel a newline on the last line");
        lex_mergeline(i);
        i--;
    }
}

static void lex_nextline(void)
{
    int i;

    int len;
    char *linestart;
    line_t *newline;

    curpos++;

    linestart = curpos;
    len = 0;
    while(*curpos && *curpos != '\n')
    {
        curpos++;
        len++;
    }

    // no point in making the line if theres nothing in it
    if(!len)
    {
        line++;
        return;
    }

    list_line_push(&lines, (line_t) {});
    newline = &lines.data[lines.len-1];

    newline->nchars = len;
    newline->text = malloc(len + 1);
    newline->linenum = malloc(sizeof(int) * (len + 1));
    newline->colnum = malloc(sizeof(int) * (len + 1));
    newline->text[len] = 0;
    newline->linenum[len] = newline->colnum[len] = -1;

    for(i=0, curpos=linestart; i<len; i++, curpos++)
    {
        newline->text[i] = *curpos;
        newline->linenum[i] = line;
        newline->colnum[i] = i;
    }

    line++;
}

static void lex_makelines(void)
{
    int i;

    curpos = srctext - 1;
    line = column = 0;

    do
        lex_nextline();
    while(*curpos);
    lex_mergelines();

    for(i=0; i<lines.len; i++)
        puts(lines.data[i].text);
}

void lex(void)
{
    list_line_init(&lines, 0);

    lex_makelines();
    lex_tokenize();

    list_line_free(&lines);
}

int tokenlen(token_t* tok)
{
    switch(tok->form)
    {
    case TOKEN_EOF:
        return 0;
    case TOKEN_IDENT:
        return strlen(tok->msg);
    case TOKEN_RID:
        return strlen(rid_strs[tok->rid]);
    case TOKEN_NUMBER:
        return strlen(tok->msg);
    case TOKEN_STRING:
        return strlen(tok->msg) + 2;
    case TOKEN_PUNC:
        return strlen(punc_strs[tok->punc]);
    case TOKEN_TYPE:
        return strlen(tok->msg);
    default:
        assert(0);
    }
}
