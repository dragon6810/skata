#include "front/front.h"

#include "ast.h"
#include "struct.h"
#include "token.h"

void front_init(void)
{
    struct_init();
}

void front_free(void)
{
    list_globaldecl_free(&ast);
    list_token_free(&tokens);
}