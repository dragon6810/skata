#include "list.h"

static void list_freestring(char** str)
{
    free(*str);
}

LIST_DEF(string)
LIST_DEF_FREE_DECONSTRUCT(string, list_freestring)
LIST_DEF(u64)
LIST_DEF_FREE(u64)