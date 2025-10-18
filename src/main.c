#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

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

    lex();

    free(srctext);
    list_token_free(&tokens);
}

void usage(char* program)
{
    printf("usage: %s <file>\n", program);
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        usage(argv[0]);
        return 1;
    }
    
    strcpy(srcpath, argv[1]);
    compile();

    return 0;
}