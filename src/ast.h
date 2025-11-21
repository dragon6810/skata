#ifndef _AST_H
#define _AST_H

#include <stdbool.h>

#include "token.h"
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
    STMNT_RETURN,
    STMNT_IF,
} stmnttype_e;

typedef enum
{
    // atoms
    EXPROP_RVAL=0,
    EXPROP_LVAL,

    // binary operators
    // use terms[0] and terms[1]
    EXPROP_ASSIGN,
    EXPROP_ADD,
    EXPROP_SUB,
    EXPROP_MULT,
    EXPROP_DIV,

    // unary operators
    EXPROP_NEG,
    EXPROP_POS,
    EXPROP_PREINC,
    EXPROP_PREDEC,
    EXPROP_POSTINC,
    EXPROP_POSTDEC,
} exprop_e;

typedef struct expr_s
{
    exprop_e op;
    union
    {
        struct expr_s *operands[2]; // binary op
        struct expr_s *operand; // unary op
        char* msg; // atom
    };
} expr_t;

typedef struct decl_s decl_t;

LIST_DECL(decl_t, decl)

struct decl_s
{
    decltype_e form;

    int type;
    char *ident;

    expr_t *expr;
    list_decl_t args;
};

typedef struct stmnt_s stmnt_t;

LIST_DECL(stmnt_t, stmnt)

typedef struct compound_s
{
    list_decl_t decls;
    list_stmnt_t stmnts;
} compound_t;

typedef struct ifstmnt_s
{
    expr_t *expr;
    stmnt_t *ifblk;
} ifstmnt_t;

typedef struct stmnt_s
{
    stmnttype_e form;

    union
    {
        expr_t *expr;
        compound_t compound;
        ifstmnt_t ifstmnt;
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

token_e parse_peekform(int offs);
const char* parse_peekstr(int offs);
const char* parse_eatform(token_e form);
void parse_eatstr(const char* str);
const char* parse_eat(void);
void parse_printexpr(const expr_t* expr);

expr_t* parse_expr(void);
void parse(void);
void dumpast(void);

#endif