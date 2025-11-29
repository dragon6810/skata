#ifndef _MAP_H
#define _MAP_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "list.h"

typedef uint64_t map_hash_t;

typedef enum
{
    MAP_EL_EMPTY=0,
    MAP_EL_FULL,
    MAP_EL_TOMB,
} map_el_e;

#define MAP_DECL(Tkey, Tval, keyname, valname) \
typedef struct map_##keyname##_##valname##_el_s \
{ \
    map_el_e state;\
    Tkey key; \
    map_hash_t hash; \
    Tval val; \
} map_##keyname##_##valname##_el_t; \
 \
typedef struct map_##keyname##_##valname##_s \
{ \
    uint64_t nbin; \
    uint64_t nfull; \
    map_##keyname##_##valname##_el_t *bins; \
} map_##keyname##_##valname##_t; \
 \
void map_##keyname##_##valname##_alloc(map_##keyname##_##valname##_t* map); \
Tval* map_##keyname##_##valname##_set(map_##keyname##_##valname##_t* map, Tkey key, Tval val); \
Tval* map_##keyname##_##valname##_get(map_##keyname##_##valname##_t* map, Tkey key); \
void map_##keyname##_##valname##_remove(map_##keyname##_##valname##_t* map, Tkey key); \
/* dst shouldn't be initialized */\
void map_##keyname##_##valname##_dup(map_##keyname##_##valname##_t* dst, map_##keyname##_##valname##_t* src); \
void map_##keyname##_##valname##_free(map_##keyname##_##valname##_t* map); \
void map_##keyname##_##valname##_freeel(map_##keyname##_##valname##_el_t* el);

// funcs can be NULL
// if hashfunc is NULL, it will cast the key to a uint64_t and use that
// keycmp should return a bool, and take two pointers to keys.
#define MAP_DEF(Tkey, Tval, keyname, valname, hashfunc, keycmp, keycpy, valcpy, keyfreefunc, valfreefunc) \
void map_##keyname##_##valname##_alloc(map_##keyname##_##valname##_t* map) \
{ \
    const int startbin = 16; \
     \
    map->nbin = startbin; \
    map->nfull = 0; \
    map->bins = calloc(startbin, sizeof(map_##keyname##_##valname##_el_t)); \
} \
 \
void map_##keyname##_##valname##_resize(map_##keyname##_##valname##_t* map) \
{ \
    int i; \
 \
    uint64_t idx; \
    map_##keyname##_##valname##_el_t *newbins; \
 \
    newbins = calloc(map->nbin * 2, sizeof(map_##keyname##_##valname##_el_t)); \
    for(i=0; i<map->nbin; i++) \
    { \
        if(map->bins[i].state != MAP_EL_FULL) \
            continue; \
 \
        idx = map->bins[i].hash % (map->nbin * 2); \
        while(newbins[idx].state == MAP_EL_FULL) \
        { \
            idx++; \
            idx %= map->nbin * 2; \
        } \
 \
        memcpy(&newbins[idx], &map->bins[i], sizeof(map_##keyname##_##valname##_el_t)); \
    } \
     \
    free(map->bins); \
    map->nbin *= 2; \
    map->bins = newbins; \
} \
 \
Tval* map_##keyname##_##valname##_set(map_##keyname##_##valname##_t* map, Tkey key, Tval val) \
{ \
    bool (*keycmpfn)(Tkey*, Tkey*) = (keycmp); \
    void (*keycpyfn)(Tkey*, Tkey*) = (keycpy); \
    void (*valcpyfn)(Tval*, Tval*) = (valcpy); \
    void (*valfreefn)(Tval*) = (valfreefunc); \
 \
    int i; \
 \
    map_hash_t hash; \
    uint64_t idx; \
 \
    if(map->nfull >= map->nbin / 2) \
        map_##keyname##_##valname##_resize(map); \
 \
    hash = hashfunc(&key); \
     \
    for(i=0, idx=hash; i<map->nbin; i++, idx++) \
    { \
        idx %= map->nbin; \
 \
        if(map->bins[idx].state == MAP_EL_TOMB) \
            continue; \
 \
        if(map->bins[idx].state == MAP_EL_EMPTY) \
        { \
            map->nfull++; \
            map->bins[idx].state = MAP_EL_FULL; \
            if(keycpyfn) \
                keycpyfn(&map->bins[idx].key, &key); \
            else \
                memcpy(&map->bins[idx].key, &key, sizeof(Tkey)); \
            if(valcpyfn) \
                valcpyfn(&map->bins[idx].val,&val); \
            else \
                memcpy(&map->bins[idx].val, &val, sizeof(Tval)); \
            map->bins[idx].hash = hash; \
\
            if(map->nfull >= map->nbin / 2) \
            { \
                map_##keyname##_##valname##_resize(map); \
                return map_##keyname##_##valname##_get(map, key); \
            } \
 \
            return &map->bins[idx].val; \
        } \
 \
        if(map->bins[idx].hash != hash) \
            continue; \
        if(keycmpfn) \
        { \
            if(!keycmpfn(&map->bins[idx].key, &key)) \
                continue; \
        } \
        else if(memcmp(&map->bins[idx].key, &key, sizeof(Tkey))) \
            continue; \
         \
        if(valfreefn) \
            valfreefn(&map->bins[idx].val); \
         \
        if(valcpyfn) \
            valcpyfn(&map->bins[idx].val, &val); \
        else \
            memcpy(&map->bins[idx].val, &val, sizeof(Tval)); \
 \
        return &map->bins[idx].val; \
    } \
\
    assert(0 && "map set failed\n");\
    return NULL;\
} \
 \
Tval* map_##keyname##_##valname##_get(map_##keyname##_##valname##_t* map, Tkey key) \
{ \
    bool (*keycmpfn)(Tkey*, Tkey*) = (keycmp); \
 \
    int i; \
 \
    map_hash_t hash; \
    uint64_t idx; \
    \
    hash = hashfunc(&key); \
     \
    for(i=0, idx=hash; i<map->nbin; i++, idx++) \
    { \
        idx %= map->nbin; \
 \
        if(map->bins[idx].state == MAP_EL_EMPTY) \
            return NULL; \
        if(map->bins[idx].state == MAP_EL_TOMB) \
            continue; \
 \
        if(map->bins[idx].hash != hash) \
            continue; \
        if(keycmpfn) \
        { \
            if(!keycmpfn(&map->bins[idx].key, &key)) \
                continue; \
        } \
        else if(memcmp(&map->bins[idx].key, &key, sizeof(Tkey))) \
            continue; \
 \
        return &map->bins[idx].val; \
    } \
 \
    return NULL; \
} \
void map_##keyname##_##valname##_remove(map_##keyname##_##valname##_t* map, Tkey key) \
{ \
    bool (*keycmpfn)(Tkey*, Tkey*) = (keycmp); \
    void (*keyfreefn)(Tkey*) = (keyfreefunc); \
    void (*valfreefn)(Tval*) = (valfreefunc); \
 \
    int i; \
 \
    map_hash_t hash; \
    uint64_t idx; \
    \
    hash = hashfunc(&key); \
     \
    for(i=0, idx=hash; i<map->nbin; i++, idx++) \
    { \
        idx %= map->nbin; \
 \
        if(map->bins[idx].state == MAP_EL_EMPTY) \
            return; \
        if(map->bins[idx].state == MAP_EL_TOMB) \
            continue; \
 \
        if(map->bins[idx].hash != hash) \
            continue; \
        if(keycmpfn) \
        { \
            if(!keycmpfn(&map->bins[idx].key, &key)) \
                continue; \
        } \
        else if(memcmp(&map->bins[idx].key, &key, sizeof(Tkey))) \
            continue; \
 \
        map->nfull--; \
        map->bins[idx].state = MAP_EL_TOMB; \
        if(keyfreefn) \
            keyfreefn(&map->bins[idx].key); \
        if(valfreefn) \
            valfreefn(&map->bins[idx].val); \
 \
        break; \
    } \
} \
 \
void map_##keyname##_##valname##_dup(map_##keyname##_##valname##_t* dst, map_##keyname##_##valname##_t* src) \
{ \
    int i; \
 \
    map_##keyname##_##valname##_alloc(dst); \
    for(i=0; i<src->nbin; i++) \
    { \
        if(src->bins[i].state != MAP_EL_FULL) \
            continue; \
        map_##keyname##_##valname##_set(dst, src->bins[i].key, src->bins[i].val); \
    } \
} \
 \
void map_##keyname##_##valname##_free(map_##keyname##_##valname##_t* map) \
{ \
    int i; \
 \
    for(i=0; i<map->nbin; i++) \
    { \
        if(map->bins[i].state != MAP_EL_FULL) \
            continue; \
        map_##keyname##_##valname##_freeel(&map->bins[i]); \
    } \
 \
    free(map->bins); \
    map->nbin = map->nfull = 0; \
    map->bins = NULL; \
} \
 \
void map_##keyname##_##valname##_freeel(map_##keyname##_##valname##_el_t* el) \
{ \
    void (*valfreefn)(Tval*) = (valfreefunc); \
    void (*keyfreefn)(Tkey*) = (keyfreefunc); \
 \
    if(keyfreefn) \
        keyfreefn(&el->key); \
    if(valfreefn) \
        valfreefn(&el->val); \
} \

MAP_DECL(char*, char*, str, str)
MAP_DECL(char*, uint64_t, str, u64)

uint64_t hash_u64(uint64_t* val);
uint64_t hash_str(char** val);

bool map_strcmp(char** a, char** b);
void map_strcpy(char** dst, char** src);
void map_freestr(char** str);

#endif