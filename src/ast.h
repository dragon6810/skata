#ifndef _AST_H
#define _AST_H

#include <stdbool.h>

#include "list.h"

typedef enum
{
    DECL_VAR=0,
    DECL_FUNC,
} decltype_e;

typedef enum
{
    STMNT_EXPR=0,
    STMNT_COMPOUND,
} stmnttype_e;

typedef enum
{
    EXPROP_ATOM=0,
    EXPROP_ADD,
    EXPROP_MULT,
} exprop_e;

typedef struct expr_s
{
    exprop_e op;
    union
    {
        struct expr_s *terms[2]; // op != EXPROP_ATOM
        char* msg; // op == EXPROP_ATOM
    };
} expr_t;

typedef struct decl_s decl_t;

LIST_DECL(decl_t, decl)

struct decl_s
{
    decltype_e form;

    int type;
    char *ident;

    list_decl_t args;
};

typedef struct stmnt_s stmnt_t;

LIST_DECL(stmnt_t, stmnt)

typedef struct compound_s
{
    list_decl_t decls;
    list_stmnt_t stmnts;
} compound_t;

typedef struct stmnt_s
{
    stmnttype_e form;

    union
    {
        expr_t *expr;
        compound_t compound;
    };
} stmnt_t;

typedef struct globaldecl_s
{
    bool hasfuncdef;

    decl_t decl;
    compound_t funcdef;
} globaldecl_t;

LIST_DECL(globaldecl_t, globaldecl)

extern list_globaldecl_t ast;

void parse_printexpr(const expr_t* expr);

expr_t* parse_expr(void);
void parse(void);

#endif