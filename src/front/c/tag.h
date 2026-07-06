#ifndef _TAG_H
#define _TAG_H

#include "map.h"

#include "struct.h"

typedef enum
{
    TAG_STRUCT=0,
    TAG_UNION,
} tagtype_e;

typedef struct tag_s
{
    char *tag;
    tagtype_e type;
    bool defined;
    union
    {
        struct_t struc;
        // TODO: unions
    };
} tag_t;

MAP_DECL(char*, tag_t*, str, ptag);

void tag_freetag(tag_t* tag);

void tag_init(void);
void tag_clear(void);
void tag_finish(void);
tag_t* tag_gettag(char* name, tagtype_e type);

#endif