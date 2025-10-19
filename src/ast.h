#ifndef _AST_H
#define _AST_H

#include <stdbool.h>

#include "list.h"

typedef enum
{
    TOPLEV_DECL=0,
    TOPLEV_FUNCDEF,
} globaltype_e;

typedef enum
{
    DECL_VAR=0,
    DECL_FUNC,
} decltype_e;

typedef struct typedvar_s
{
    int type;
    char* ident;
} typedvar_t;

LIST_DECL(typedvar_t, typedvar)

typedef struct decl_s
{
    decltype_e form;

    int type;
    char *ident;

    list_typedvar_t args;
} decl_t;

LIST_DECL(decl_t, decl)

typedef struct funcdef_s
{
    list_decl_t decls;
} funcdef_t;

typedef struct globaldecl_s
{
    bool hasfuncdef;

    decl_t decl;
    funcdef_t funcdef;
} globaldecl_t;

LIST_DECL(globaldecl_t, globaldecl)

extern list_globaldecl_t ast;

void parse(void);

#endif