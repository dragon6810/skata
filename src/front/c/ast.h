#ifndef _AST_H
#define _AST_H

#include <stdbool.h>

#include "list.h"
#include "token.h"
#include "type.h"

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
    STMNT_WHILE,
} stmnttype_e;

typedef enum
{
    // atoms
    EXPROP_LIT=0,
    EXPROP_VAR,
    EXPROP_FUNC,

    // ternary operators
    EXPROP_COND, // ? :

    // binary operators
    // use terms[0] and terms[1]
    EXPROP_ASSIGN, // =
    EXPROP_ADD, // +
    EXPROP_SUB, // -
    EXPROP_MULT, // *
    EXPROP_DIV, // /

    // unary operators
    EXPROP_NEG, // -
    EXPROP_POS, // +
    EXPROP_PREINC, // ++
    EXPROP_PREDEC, // --
    EXPROP_POSTINC, // ++
    EXPROP_POSTDEC, // --
    EXPROP_CAST, // (<type>)

    // variadic operators
    EXPROP_CALL, // ( ... )
} exprop_e;

// -1 is variadic, 0 is atom
static const int exprop_nop[] = 
{
    0,
    0,
    3,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    -1,
};

typedef struct expr_s expr_t;

LIST_DECL(expr_t*, pexpr)

struct expr_s
{
    int line, col;

    exprop_e op;
    type_t type;
    bool lval; // if false, rval
    union
    {
        expr_t *operands[3]; // binary and trinary op
        expr_t *operand; // unary op
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        char* msg; // atom
    };
    
    union
    {
        list_pexpr_t variadic; // function call
        type_t casttype; // cast
    };
};

typedef struct decl_s decl_t;

LIST_DECL(decl_t, decl)
LIST_DECL(decl_t*, pdecl)

struct decl_s
{
    decltype_e form;

    type_t type;
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

typedef struct whilestmnt_s
{
    expr_t *expr;
    stmnt_t *body;
} whilestmnt_t;

typedef struct stmnt_s
{
    stmnttype_e form;

    union
    {
        expr_t *expr;
        compound_t compound;
        ifstmnt_t ifstmnt;
        whilestmnt_t whilestmnt;
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

void parse_type(type_t* type);
bool parse_istype();
token_e parse_peekform(int offs);
const char* parse_peekstr(int offs);
int parse_getline(void);
int parse_getcol(void);
const char* parse_eatform(token_e form);
bool parse_eatstr(const char* str);
const char* parse_eat(void);
void parse_printexpr(const expr_t* expr);

expr_t* parse_expr(void);

#endif