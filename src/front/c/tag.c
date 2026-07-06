#include "tag.h"

#include "struct.h"

void tag_cpytag(tag_t* dst, tag_t* src)
{
    *dst = *src;
    dst->tag = strdup(src->tag);
    struct_cpy(&dst->struc, &src->struc);
}

void tag_freetag(tag_t* tag)
{
    free(tag->tag);
    struct_free(&tag->struc);
}

MAP_DEF(char*, tag_t, str, tag, hash_str, map_strcmp, map_strcpy, NULL, map_freestr, tag_freetag)

map_str_tag_t tags;

void tag_init(void)
{
    map_str_tag_alloc(&tags);
}

void tag_clear(void)
{
    tag_finish();
    tag_init();
}

void tag_finish(void)
{
    map_str_tag_free(&tags);
}

tag_t* tag_gettag(char* name, tagtype_e type)
{
    tag_t *tag;

    tag = map_str_tag_get(&tags, name);
    if(tag)
    {
        if(tag->type == type)
            return tag;
        assert(0); // TODO: error message for tag that is trying to be made for a union and a struct
    }

    tag = map_str_tag_set(&tags, name, (tag_t){});
    tag->tag = strdup(name);
    tag->type = type;
    tag->defined = false;
    return tag;
}