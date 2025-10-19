#include "type.h"

list_string_t types;

void type_init(void)
{
    list_string_init(&types, 0);

    type_register("void");
    type_register("char");
    type_register("short");
    type_register("int");
    type_register("long");
    type_register("float");
    type_register("double");
}

int type_register(const char* name)
{
    list_string_push(&types, strdup(name));
    return types.len - 1;
}

int type_find(const char* name)
{
    int i;

    for(i=0; i<types.len; i++)
        if(!strcmp(types.data[i], name))
            return i;

    return -1;
}

void type_free(void)
{
    list_string_free(&types);
}