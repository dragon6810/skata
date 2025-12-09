#ifndef _TOKEN_H
#define _TOKEN_H

#include "list.h"

typedef enum
{
    TOKEN_EOF=0,
    TOKEN_IDENT,
    TOKEN_RID,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_PUNC,
    
    // no tokens will have these types
    // these are discovered during parsing
    TOKEN_TYPE,
} token_e;

#define DECL_PUNC(id, str)
#define DECL_RID(id, str) RID_##id,
typedef enum
{
#include "tokens.def"
RID_COUNT,
} rid_e;
#undef DECL_RID

#define DECL_RID(id, str) str,
static const char* rid_strs[] =
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
PUNC_COUNT,
} punc_e;
#undef DECL_PUNC

#define DECL_PUNC(id, str) str,
static const char* punc_strs[] =
{
#include "tokens.def"
};
#undef DECL_PUNC
#undef DECL_RID

typedef struct token_s
{
    int line;
    int column;

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

LIST_DECL(token_t, token)

extern char* srctext;
extern list_token_t tokens;

void lex(void);
int tokenlen(token_t* tok);

#endif