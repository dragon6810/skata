#ifndef _MAP_H
#define _MAP_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "list.h"

typedef uint64_t map_hash_t;

#define MAP_DECL(Tkey, Tval, keyname, valname) \
typedef struct map_##keyname##_##valname##_el_s \
{ \
    bool full; \
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
Tval* map_##keyname##_##valname##_set(map_##keyname##_##valname##_t* map, Tkey* key, Tval* val); \
Tval* map_##keyname##_##valname##_get(map_##keyname##_##valname##_t* map, Tkey* key); \
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
        if(!map->bins[i].full) \
            continue; \
 \
        idx = map->bins[i].hash % (map->nbin * 2); \
        while(newbins[idx].full) \
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
Tval* map_##keyname##_##valname##_set(map_##keyname##_##valname##_t* map, Tkey* key, Tval* val) \
{ \
    map_hash_t (*hashfn)(Tkey*) = (hashfunc); \
    bool (*keycmpfn)(Tkey*, Tkey*) = (keycmp); \
    void (*keycpyfn)(Tkey*, Tkey*) = (keycpy); \
    void (*valcpyfn)(Tval*, Tval*) = (valcpy); \
    void (*valfreefn)(Tval*) = (valfreefunc); \
 \
    map_hash_t hash; \
    uint64_t idx; \
 \
    if(hashfn) \
        hash = hashfn(key); \
    else \
        hash = (map_hash_t) *key; \
     \
    idx = hash % map->nbin; \
    for(idx=hash%map->nbin; idx<hash%map->nbin+map->nbin; idx++) \
    { \
        if(!map->bins[idx%map->nbin].full) \
        { \
            map->nfull++; \
            map->bins[idx%map->nbin].full = true; \
            if(keycpyfn) \
                keycpyfn(&map->bins[idx%map->nbin].key, key); \
            else \
                memcpy(&map->bins[idx%map->nbin].key, key, sizeof(*key)); \
            if(valcpyfn) \
                valcpyfn(&map->bins[idx%map->nbin].val, val); \
            else \
                memcpy(&map->bins[idx%map->nbin].val, val, sizeof(*val)); \
            map->bins[idx%map->nbin].hash = hash; \
\
            if(map->nfull >= map->nbin / 2)\
            {\
                map_##keyname##_##valname##_resize(map);\
                return map_##keyname##_##valname##_get(map, key);\
            }\
 \
            return &map->bins[idx%map->nbin].val; \
        } \
 \
        if(map->bins[idx%map->nbin].hash != hash) \
            continue; \
        if(keycmpfn) \
        { \
            if(!keycmpfn(&map->bins[idx%map->nbin].key, key)) \
                continue; \
        } \
        else if(map->bins[idx%map->nbin].key != *key) \
            continue; \
         \
        if(valfreefn) \
            valfreefn(&map->bins[idx%map->nbin].val); \
         \
        if(valcpyfn) \
            valcpyfn(&map->bins[idx%map->nbin].val, val); \
        else \
            map->bins[idx%map->nbin].val = *val; \
 \
        return &map->bins[idx%map->nbin].val; \
    } \
\
    assert(0 && "map set failed\n");\
    return NULL;\
} \
 \
Tval* map_##keyname##_##valname##_get(map_##keyname##_##valname##_t* map, Tkey* key) \
{ \
    map_hash_t (*hashfn)(Tkey*) = (hashfunc); \
    bool (*keycmpfn)(Tkey*, Tkey*) = (keycmp); \
 \
    map_hash_t hash; \
    uint64_t idx; \
    \
    if(hashfn) \
        hash = hashfn(key); \
    else \
        hash = (map_hash_t) *key; \
     \
    for(idx=hash%map->nbin; idx<hash%map->nbin+map->nbin; idx++) \
    { \
        if(!map->bins[idx%map->nbin].full) \
            return NULL; \
        if(map->bins[idx%map->nbin].hash != hash) \
            continue; \
        if(keycmpfn) \
        { \
            if(!keycmpfn(&map->bins[idx%map->nbin].key, key)) \
                continue; \
        } \
        else if(map->bins[idx%map->nbin].key != *key) \
            continue; \
 \
        return &map->bins[idx%map->nbin].val; \
    } \
 \
    return NULL; \
} \
 \
void map_##keyname##_##valname##_free(map_##keyname##_##valname##_t* map) \
{ \
    int i; \
 \
    for(i=0; i<map->nbin; i++) \
    { \
        if(!map->bins[i].full) \
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

uint64_t map_strhash(char** str);
bool map_strcmp(char** a, char** b);
void map_strcpy(char** dst, char** src);
void map_freestr(char** str);

#endif