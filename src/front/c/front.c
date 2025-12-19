#include "front/front.h"

#include "ast.h"
#include "token.h"

void front_free(void)
{
    list_globaldecl_free(&ast);
    list_token_free(&tokens);
}