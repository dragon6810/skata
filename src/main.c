#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "map.h"
#include "token.h"
#include "type.h"

char srcpath[PATH_MAX];

void compile(void)
{
    FILE *ptr;
    int len;

    ptr = fopen(srcpath, "r");
    if(!ptr)
    {
        fprintf(stderr, "file inaccessible \"%s\".\n", srcpath);
        exit(1);
    }

    fseek(ptr, 0, SEEK_END);
    len = ftell(ptr);
    fseek(ptr, 0, SEEK_SET);

    srctext = malloc(len + 1);
    fread(srctext, 1, len, ptr);
    srctext[len] = 0;
    
    fclose(ptr);

    type_init();

    lex();
    parse();

    free(srctext);
    list_token_free(&tokens);
    type_free();
}

void usage(char* program)
{
    printf("usage: %s <file>\n", program);
}

int main(int argc, char** argv)
{
    int key, val;
    map_int_int_t map;

    map_int_int_alloc(&map);
    key = 4; val = 2;
    map_int_int_set(&map, &key, &val);
    printf("map[%d]: %d\n", key, *map_int_int_get(&map, &key));
    key = 2; val = 3;
    map_int_int_set(&map, &key, &val);
    printf("map[%d]: %d\n", key, *map_int_int_get(&map, &key));
    key = 99999; val = 63;
    map_int_int_set(&map, &key, &val);
    printf("map[%d]: %d\n", key, *map_int_int_get(&map, &key));
    map_int_int_free(&map);

    if(argc != 2)
    {
        usage(argv[0]);
        return 1;
    }
    
    strcpy(srcpath, argv[1]);
    compile();

    return 0;
}