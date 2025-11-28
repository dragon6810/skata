#include "set.h"

#include "map.h"

static uint64_t u64hash(uint64_t* val)
{
    return *val;
}

SET_DEF(char*, str, map_strhash, map_strcmp, map_strcpy, map_freestr)
SET_DEF(uint64_t, u64, u64hash, NULL, NULL, NULL)