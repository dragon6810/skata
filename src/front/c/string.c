#include "string.h"

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

void string_process(void)
{

}