#include "front/front.h"

#include "ast.h"
#include "struct.h"
#include "tag.h"
#include "token.h"

void front_init(void)
{
    tag_init();
    struct_init();
}

void front_free(void)
{
    tag_finish();
    list_globaldecl_free(&ast);
    list_token_free(&tokens);
}