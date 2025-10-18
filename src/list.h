#ifndef _LIST_H
#define _LIST_H

#include <stdint.h>
#include <stdlib.h>

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
\

#define LIST_DEF_FREE(name) \
void list_##name##_free(list_##name##_t* list)\
{\
    list->len = list->cap = 0;\
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
    free(list->data);\
    list->data = NULL;\
}

#endif