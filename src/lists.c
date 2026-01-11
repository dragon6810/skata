#include "list.h"

static void list_freestring(char** str)
{
    free(*str);
}

static void list_freestrpair(strpair_t* strpair)
{
    free(strpair->a);
    free(strpair->b);
}

LIST_DEF(string)
LIST_DEF_FREE_DECONSTRUCT(string, list_freestring)
LIST_DEF(strpair)
LIST_DEF_FREE_DECONSTRUCT(strpair, list_freestrpair)
LIST_DEF(u64)
LIST_DEF_FREE(u64)
LIST_DEF(set_str)
LIST_DEF_FREE_DECONSTRUCT(set_str, set_str_free)