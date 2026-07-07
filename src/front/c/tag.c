#include "tag.h"

#include "struct.h"

void tag_freetag(tag_t* tag)
{
    free(tag->tag);
    struct_free(&tag->struc);
}

static void tag_freeptag(tag_t** tag)
{
    tag_freetag(*tag);
    free(*tag);
}

MAP_DEF(char*, tag_t*, str, ptag, hash_str, map_strcmp, map_strcpy, NULL, map_freestr, tag_freeptag)

map_str_ptag_t tags;

void tag_init(void)
{
    map_str_ptag_alloc(&tags);
}

void tag_clear(void)
{
    tag_finish();
    tag_init();
}

void tag_finish(void)
{
    map_str_ptag_free(&tags);
}

tag_t* tag_gettag(const char* name, tagtype_e type)
{
    tag_t **el, *tag;

    // the map only reads the key on get, and copies it on set
    el = map_str_ptag_get(&tags, (char*) name);
    if(el)
    {
        tag = *el;
        if(tag->type == type)
            return tag;
        assert(0); // TODO: error message for tag that is trying to be made for a union and a struct
    }

    tag = calloc(1, sizeof(tag_t));
    tag->tag = strdup(name);
    tag->type = type;
    tag->defined = false;
    map_str_ptag_set(&tags, (char*) name, tag);

    return tag;
}
