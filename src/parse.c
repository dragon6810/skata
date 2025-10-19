#include "ast.h"

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

void parse(void)
{
    
}
