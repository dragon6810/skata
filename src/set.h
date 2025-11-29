#ifndef _SET_H
#define _SET_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    SET_EL_EMPTY=0,
    SET_EL_FULL,
    SET_EL_TOMB,
} set_elstate_e;

#define SET_DECL(T, name)\
\
typedef struct set_##name##_el_s\
{\
    set_elstate_e state;\
    uint64_t hash;\
    T val;\
} set_##name##_el_t;\
\
typedef struct set_##name##_s\
{\
    uint64_t nbin;\
    uint64_t nfull;\
    set_##name##_el_t *bins;\
} set_##name##_t;\
\
void set_##name##_alloc(set_##name##_t* set);\
void set_##name##_add(set_##name##_t* set, T val);\
void set_##name##_remove(set_##name##_t* set, T val);\
bool set_##name##_contains(set_##name##_t* set, T val);\
/* dst shouldn't be initialized */\
void set_##name##_dup(set_##name##_t* dst, set_##name##_t* src);\
void set_##name##_clear(set_##name##_t* set);\
/* stores the union in a*/\
void set_##name##_union(set_##name##_t* a, set_##name##_t* b);\
/* stores the intersection in a*/\
void set_##name##_intersection(set_##name##_t* a, set_##name##_t* b);\
bool set_##name##_isequal(const set_##name##_t* a, set_##name##_t* b);\
void set_##name##_free(set_##name##_t* set);

#define SET_DEF(T, name, hashfunc, cmpfunc, cpyfunc, freefunc)\
void set_##name##_alloc(set_##name##_t* set)\
{\
    const int startbin = 16;\
\
    set->nbin = startbin;\
    set->nfull = 0;\
    set->bins = calloc(startbin, sizeof(set_##name##_el_t));\
}\
\
void set_##name##_resize(set_##name##_t* set) \
{\
    int i;\
\
    uint64_t idx;\
    set_##name##_el_t *newbins;\
\
    newbins = calloc(set->nbin * 2, sizeof(set_##name##_el_t)); \
    for(i=0; i<set->nbin; i++)\
    {\
        if(set->bins[i].state != SET_EL_FULL)\
            continue;\
\
        idx = set->bins[i].hash % (set->nbin * 2);\
        while(newbins[idx].state == SET_EL_FULL)\
        {\
            idx++;\
            idx %= set->nbin * 2;\
        }\
\
        memcpy(&newbins[idx], &set->bins[i], sizeof(set_##name##_el_t));\
    }\
    \
    free(set->bins);\
    set->nbin *= 2;\
    set->bins = newbins;\
}\
\
void set_##name##_add(set_##name##_t* set, T val)\
{\
    void (*cpyfn)(T*, T*) = (cpyfunc); \
\
    int i;\
\
    uint64_t hash, idx;\
\
    if(set->nfull >= set->nbin / 2)\
        set_##name##_resize(set);\
\
    if(set_##name##_contains(set, val))\
        return;\
\
    hash = hashfunc(&val);\
    for(i=0, idx=hash; i<set->nbin; i++, idx++)\
    {\
        idx = idx % set->nbin;\
\
        if(set->bins[idx].state == SET_EL_FULL)\
            continue;\
\
        set->nfull++;\
        set->bins[idx].state = SET_EL_FULL;\
        set->bins[idx].hash = hash;\
        if(cpyfn)\
            cpyfn(&set->bins[idx].val, &val);\
        else\
            memcpy(&set->bins[idx].val, &val, sizeof(T));\
\
        if(set->nfull >= set->nbin / 2)\
            set_##name##_resize(set);\
\
        return;\
    }\
}\
void set_##name##_remove(set_##name##_t* set, T val)\
{\
    bool (*cmpfn)(T*, T*) = (cmpfunc);\
    void (*freefn)(T*) = (freefunc);\
\
    int i;\
\
    uint64_t hash, idx;\
\
    hash = hashfunc(&val);\
    for(i=0, idx=hash; i<set->nbin; i++, idx++)\
    {\
        idx = idx % set->nbin;\
\
        if(set->bins[idx].state == SET_EL_EMPTY)\
            return;\
        if(set->bins[idx].state == SET_EL_TOMB)\
            continue;\
        if(set->bins[idx].hash != hash)\
            continue;\
        if(cmpfn && !cmpfn(&set->bins[idx].val, &val))\
            continue;\
        if(!cmpfn && memcmp(&set->bins[idx].val, &val, sizeof(T)))\
            continue;\
\
        set->nfull--;\
        set->bins[idx].state = SET_EL_TOMB;\
        if(freefn)\
            freefn(&set->bins[idx].val);\
\
        return;\
    }\
}\
bool set_##name##_contains(set_##name##_t* set, T val)\
{\
    bool (*cmpfn)(T*, T*) = (cmpfunc);\
\
    int i;\
\
    uint64_t hash, idx;\
\
    hash = hashfunc(&val);\
    for(i=0, idx=hash; i<set->nbin; i++, idx++)\
    {\
        idx = idx % set->nbin;\
\
        if(set->bins[idx].state == SET_EL_EMPTY)\
            return false;\
        if(set->bins[idx].state == SET_EL_TOMB)\
            continue;\
        if(set->bins[idx].hash != hash)\
            continue;\
        if(cmpfn && !cmpfn(&set->bins[idx].val, &val))\
            continue;\
        if(!cmpfn && memcmp(&set->bins[idx].val, &val, sizeof(T)))\
            continue;\
\
        return true;\
    }\
\
    return false;\
}\
void set_##name##_dup(set_##name##_t* dst, set_##name##_t* src)\
{\
    int i;\
\
    set_##name##_alloc(dst);\
    for(i=0; i<src->nbin; i++)\
        if(src->bins[i].state == SET_EL_FULL)\
            set_##name##_add(dst, src->bins[i].val);\
}\
void set_##name##_clear(set_##name##_t* set)\
{\
    set_##name##_free(set);\
    set_##name##_alloc(set);\
}\
void set_##name##_union(set_##name##_t* a, set_##name##_t* b)\
{\
    int i;\
\
    for(i=0; i<b->nbin; i++)\
        if(b->bins[i].state == SET_EL_FULL)\
            set_##name##_add(a, b->bins[i].val);\
}\
void set_##name##_intersection(set_##name##_t* a, set_##name##_t* b)\
{\
    void (*freefn)(T*) = (freefunc);\
\
    int i;\
\
    for(i=0; i<a->nbin; i++)\
    {\
        if(a->bins[i].state != SET_EL_FULL)\
            continue;\
        if(set_##name##_contains(b, a->bins[i].val))\
            continue;\
        a->nfull--;\
        a->bins[i].state = SET_EL_TOMB;\
        if(freefn)\
            freefn(&a->bins[i].val);\
    }\
}\
bool set_##name##_isequal(const set_##name##_t* a, set_##name##_t* b)\
{\
    int i;\
\
    if(a->nfull != b->nfull)\
        return false;\
\
    for(i=0; i<a->nbin; i++)\
    {\
        if(a->bins[i].state != SET_EL_FULL)\
            continue;\
        if(set_##name##_contains(b, a->bins[i].val))\
            continue;\
        return false;\
    }\
\
    return true;\
}\
void set_##name##_free(set_##name##_t* set)\
{\
    void (*freefn)(T*) = (freefunc);\
\
    int i;\
\
    if(freefn)\
    {\
        for(i=0; i<set->nbin; i++)\
            if(set->bins[i].state == SET_EL_FULL)\
                freefn(&set->bins[i].val);\
    }\
\
    free(set->bins);\
    set->nfull = 0;\
    set->nbin = 0;\
    set->bins = NULL;\
}

SET_DECL(char*, str)
SET_DECL(uint64_t, u64)

uint64_t hash_ptr(void** val);

#endif