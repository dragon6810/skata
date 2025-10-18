#ifndef _AST_H
#define _AST_H

typedef enum
{
    TOPLEV_DECL=0,
    TOPLEV_FUNCDEF,
} toplevtype_e;

typedef struct funcdef_s
{

} funcdef_t;

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

typedef struct decl_s
{
    decltype_e form;

    int type;
    char *ident;

    int nargs;
    typedvar_t *args;
} decl_t;

typedef struct toplevdecl_s
{
    toplevtype_e form;

    union
    {
        decl_t decl;
        funcdef_t funcdef;
    };

    struct toplevdecl_s *next;
} toplevdecl_t;

typedef struct ast_s
{

} ast_t;

#endif