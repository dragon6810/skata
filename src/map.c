#include "map.h"

#include <math.h>
#include <stdio.h>

static uint64_t map_u64pow(uint64_t base, uint64_t exp)
{
    uint16_t res;

    res = 1;
    while(exp)
    {
        if(exp % 2)
            res *= base;
        base *= base;
        exp /= 2;
    }

    return res;
}

map_hash_t map_strhash(char** str)
{
    const uint64_t p = 31;

    int i;

    const char* s;
    int len;
    map_hash_t hash;

    s = *str;
    len = strlen(s);

    for(i=0, hash=0; i<len; i++)
        hash += (uint64_t) s[i] * map_u64pow(p, i);

    return hash;
}

bool map_strcmp(char** a, char** b)
{
    return !strcmp(*a, *b);
}

void map_strcpy(char** dst, char** src)
{
    *dst = strdup(*src);
}

void map_freestr(char** str)
{
    free(*str);
}

MAP_DEF(char*, char*, str, str, map_strhash, map_strcmp, map_strcpy, map_strcpy, map_freestr, map_freestr)
MAP_DEF(char*, uint64_t, str, u64, map_strhash, map_strcmp, map_strcpy, NULL, map_freestr, NULL)