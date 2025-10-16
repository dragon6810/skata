#ifndef _TOKEN_H
#define _TOKEN_H

typedef enum
{
    TOKEN_EOF=0,
    TOKEN_IDENT,
    TOKEN_RID,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_PUNC,
} token_e;

#define DECL_RID(id, str) RID_##id,
#define DECL_PUNC(id, str)
typedef enum
{
#include "tokens.def"
} rid_e;
#undef DECL_RID
#undef DECL_PUNC

#define DECL_RID(id, str) str,
#define DECL_PUNC(id, str)
static const char* rid_strs =
{
#include "tokens.def"
};
#undef DECL_RID
#undef DECL_PUNC

#define DECL_RID(id, str)
#define DECL_PUNC(id, str) PUNC_##id,
typedef enum
{
#include "tokens.def"
} punc_e;
#undef DECL_RID
#undef DECL_PUNC

#define DECL_RID(id, str)
#define DECL_PUNC(id, str) str,
static const char* punc_strs =
{
#include "tokens.def"
};
#undef DECL_RID
#undef DECL_PUNC

typedef struct token_s
{
    token_e form;
    union
    {
        // TOKEN_IDENT, TOKEN_NUMBER, TOKEN_STRING
        char *msg;
        // TOKEN_RID
        rid_e rid;
        // TOKEN_PUNC
        punc_e punc;
    };
} token_t;

#endif