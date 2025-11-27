#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "flags.h"
#include "ir.h"
#include "map.h"
#include "asmgen.h"
#include "token.h"
#include "type.h"

bool emitast = false;
bool emitir = false;
bool emitflow = false;
bool emitdomtree = false;
bool emitssa = false;
bool emitlowered = false;

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
    free(srctext);

    parse();
    if(emitast)
    {
        dumpast();

        list_globaldecl_free(&ast);
        list_token_free(&tokens);
        type_free();
        return;
    }
    
    ir_gen();
    if(emitir)
    {
        ir_dump();

        ir_free();
        list_globaldecl_free(&ast);
        list_token_free(&tokens);
        type_free();
        return;
    }

    ir_flow();
    if(emitflow)
    {
        ir_dumpflow();

        ir_free();
        list_globaldecl_free(&ast);
        list_token_free(&tokens);
        type_free();
        return;
    }
    if(emitdomtree)
    {
        ir_dumpdomtree();

        ir_free();
        list_globaldecl_free(&ast);
        list_token_free(&tokens);
        type_free();
        return;
    }

    ir_ssa();
    if(emitssa)
    {
        ir_dump();

        ir_free();
        list_globaldecl_free(&ast);
        list_token_free(&tokens);
        type_free();
        return;
    }

    ir_lower();
    if(emitlowered)
    {
        ir_dump();

        ir_free();
        list_globaldecl_free(&ast);
        list_token_free(&tokens);
        type_free();
        return;
    }
    
    asmgen_arm();
    
    ir_free();
    list_token_free(&tokens);
    type_free();
}

void usage(char* program)
{
    printf("usage: %s [-emit-ast] [-emit-ir] <file>\n", program);
}

int main(int argc, char** argv)
{
    int i;

    if(argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    for(i=1; i<argc-1; i++)
    {
        if(argv[i][0] != '-')
            break;
        if(!strcmp(argv[i], "-emit-ast"))
            emitast = true;
        else if(!strcmp(argv[i], "-emit-ir"))
            emitir = true;
        else if(!strcmp(argv[i], "-emit-flow"))
            emitflow = true;
        else if(!strcmp(argv[i], "-emit-domtree"))
            emitdomtree = true;
        else if(!strcmp(argv[i], "-emit-ssa"))
            emitssa = true;
        else if(!strcmp(argv[i], "-emit-lowered"))
            emitlowered = true;
        else
        {
            usage(argv[0]);
            return 1;
        }
    }
    
    for(; i<argc; i++)
    {
        strcpy(srcpath, argv[i]);
        compile();
    }

    return 0;
}
