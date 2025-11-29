#include "set.h"

#include "map.h"

SET_DEF(char*, str, hash_str, map_strcmp, map_strcpy, map_freestr)
SET_DEF(uint64_t, u64, hash_u64, NULL, NULL, NULL)

uint64_t hash_ptr(void** val)
{
    return (uint64_t) *val;
}