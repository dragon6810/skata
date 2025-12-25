#ifndef _LIST_H
#define _LIST_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define U64_ROUNDUPTOPOW2(len)\
len--;\
len |= len >> 1;\
len |= len >> 2;\
len |= len >> 4;\
len |= len >> 8;\
len |= len >> 16;\
len |= len >> 32;\
len++

#define LIST_DECL(T, name) \
\
typedef struct list_##name##_s\
{\
    uint64_t len;\
    uint64_t cap;\
    T *data;\
} list_##name##_t;\
\
void list_##name##_init(list_##name##_t* list, uint64_t len);\
void list_##name##_ppush(list_##name##_t* list, T* val);\
void list_##name##_push(list_##name##_t* list, T val);\
void list_##name##_resize(list_##name##_t* list, uint64_t len);\
void list_##name##_remove(list_##name##_t* list, uint64_t idx);\
void list_##name##_insert(list_##name##_t* list, uint64_t idx, T val);\
T* list_##name##_find(list_##name##_t* list, T val);\
void list_##name##_dup(list_##name##_t* dst, list_##name##_t* src);\
void list_##name##_clear(list_##name##_t* list);\
void list_##name##_reverse(list_##name##_t* list);\
void list_##name##_free(list_##name##_t* list);

#define LIST_DEF(name) \
\
void list_##name##_init(list_##name##_t* list, uint64_t len)\
{\
    list->len = len;\
    list->cap = len;\
    if(len)\
    {\
        list->data = malloc(sizeof(*list->data) * len);\
        memset(list->data, 0, sizeof(*list->data) * len);\
    }\
    else\
        list->data = NULL;\
}\
void list_##name##_ppush(list_##name##_t* list, typeof(*list->data)* val)\
{\
    if(list->len + 1 > list->cap)\
    {\
        list->cap *= 2;\
        if(!list->cap)\
            list->cap = 1;\
        if(list->data)\
            list->data = realloc(list->data, sizeof(*list->data) * list->cap);\
        else\
            list->data = malloc(sizeof(*list->data) * list->cap);\
    }\
\
    list->data[list->len++] = *val;\
}\
void list_##name##_push(list_##name##_t* list, typeof(*list->data) val)\
{\
    if(list->len + 1 > list->cap)\
    {\
        list->cap *= 2;\
        if(!list->cap)\
            list->cap = 1;\
        if(list->data)\
            list->data = realloc(list->data, sizeof(*list->data) * list->cap);\
        else\
            list->data = malloc(sizeof(*list->data) * list->cap);\
    }\
\
    list->data[list->len++] = val;\
}\
void list_##name##_resize(list_##name##_t* list, uint64_t len)\
{\
    uint64_t cap;\
    typeof(list->data) newdata;\
\
    cap = len;\
    U64_ROUNDUPTOPOW2(cap);\
\
    newdata = malloc(sizeof(*list->data) * cap);\
    if(len <= list->len)\
        memcpy(newdata, list->data, sizeof(*list->data) * len);\
    else\
        memcpy(newdata, list->data, sizeof(*list->data) * list->len);\
\
    list_##name##_free(list);\
    list->len = len;\
    list->cap = cap;\
    list->data = newdata;\
}\
void list_##name##_remove(list_##name##_t* list, uint64_t idx)\
{\
    if(idx + 1 < list->len)\
    {\
        memmove\
        (\
            &list->data[idx],\
            &list->data[idx + 1],\
            (list->len - idx - 1) * sizeof(*list->data)\
        );\
    }\
\
    list->len--;\
}\
void list_##name##_insert(list_##name##_t* list, uint64_t idx, typeof(*list->data) val)\
{\
    if (list->len + 1 > list->cap)\
    {\
        list->cap = list->cap ? list->cap * 2 : 1;\
        if (list->data)\
            list->data = realloc(list->data, sizeof(*list->data) * list->cap);\
        else\
            list->data = malloc(sizeof(*list->data) * list->cap);\
    }\
\
    if (idx < list->len)\
    {\
        memmove\
        (\
            &list->data[idx + 1],\
            &list->data[idx],\
            (list->len - idx) * sizeof(*list->data)\
        );\
    }\
\
    list->data[idx] = val;\
    list->len++;\
}\
typeof(((list_##name##_t*)0)->data) list_##name##_find(list_##name##_t* list, typeof(*list->data) val)\
{\
    int i;\
\
    for(i=0; i<list->len; i++)\
        if(!memcmp(&list->data[i], &val, sizeof(val)))\
            return &list->data[i];\
\
    return NULL;\
}\
void list_##name##_dup(list_##name##_t* dst, list_##name##_t* src)\
{\
    memcpy(dst, src, sizeof(list_##name##_t));\
    dst->data = malloc(dst->cap * sizeof(*dst->data));\
    memcpy(dst->data, src->data, dst->len * sizeof(*dst->data));\
}\
void list_##name##_clear(list_##name##_t* list)\
{\
    list_##name##_free(list);\
    list_##name##_init(list, 0);\
}\
void list_##name##_reverse(list_##name##_t* list)\
{\
    int i, j;\
    typeof(*list->data) temp;\
\
    if (list->len <= 1)\
        return;\
\
    for(i=0, j=list->len-1; i<j; i++, j--)\
    {\
        temp = list->data[i];\
        list->data[i] = list->data[j];\
        list->data[j] = temp;\
    }\
}\
\

#define LIST_DEF_FREE(name) \
void list_##name##_free(list_##name##_t* list)\
{\
    list->len = list->cap = 0;\
    if(list->data)\
        free(list->data);\
    list->data = NULL;\
}

#define LIST_DEF_FREE_DECONSTRUCT(name, deconstructor) \
void list_##name##_free(list_##name##_t* list)\
{\
    uint64_t i;\
\
    for(i=0; i<list->len; i++)\
        deconstructor(&list->data[i]);\
    list->len = list->cap = 0;\
    if(list->data)\
        free(list->data);\
    list->data = NULL;\
}

typedef struct strpair_s
{
    char *a;
    char *b;
} strpair_t;

LIST_DECL(char*, string)
LIST_DECL(strpair_t, strpair)
LIST_DECL(uint64_t, u64)

#endif