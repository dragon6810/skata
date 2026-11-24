#include "string.h"

#include "front/error.h"
#include "token.h"

void string_concat(void)
{
    int i;

    token_t *tok;
    char *str1, *str2, *str;

    for(i=0; i<tokens.len; i++)
    {
        tok = &tokens.data[i];
        if(tok->form == TOKEN_EOF)
            break;

        if(tok->form != TOKEN_STRING || (tok+1)->form != TOKEN_STRING)
            continue;

        str1 = tok->msg;
        str2 = (tok+1)->msg;
        str = malloc(strlen(str1) + strlen(str2) + 1);
        memcpy(str, str1, strlen(str1));
        memcpy(str + strlen(str1), str2, strlen(str2));
        str[strlen(str1) + strlen(str2)] = 0;

        free(str1);
        free(str2);
        tok->msg = str;
        list_token_remove(&tokens, i+1);
    }
}

static void string_procstring(token_t* tok)
{
    char *c;

    list_char_t newstr;

    for(c=tok->msg; *c; c++)
        if(*c == '\\')
            break;
    if(!*c)
        return;
    
    list_char_init(&newstr, 0);
    for(c=tok->msg; *c; c++)
    {
        if(*c != '\\')
        {
            list_char_push(&newstr, *c);
            continue;
        }

        switch(*(c+1))
        {
        case 'n':
            list_char_push(&newstr, '\n');
            c++;
            break;
        default:
            error(true, tok->line, tok->column + 1 + (c-tok->msg), "unrecognized escape sequence\n");
            break;
        }
    }

    free(tok->msg);
    tok->msg = malloc(newstr.len + 1);
    memcpy(tok->msg, newstr.data, newstr.len);
    tok->msg[newstr.len] = 0;
    list_char_free(&newstr);
}

void string_process(void)
{
    int i;
    token_t *tok;

    for(i=0, tok=tokens.data; i<tokens.len; i++, tok++)
    {
        if(tok->form == TOKEN_EOF)
            break;
        if(tok->form != TOKEN_STRING)
            continue;

        string_procstring(tok);
    }
}