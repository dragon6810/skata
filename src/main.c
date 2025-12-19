#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "back/back.h"
#include "back/regalloc.h"
#include "flags.h"
#include "front/front.h"
#include "map.h"
#include "middle/ir.h"
#include "middle/middle.h"

bool emitast = false;
bool emitir = false;
bool emitflow = false;
bool emitdomtree = false;
bool emitssa = false;
bool emitbackir = false;
bool emitlowered = false;
bool emitreggraph = false;

char srcpath[PATH_MAX];

char* loadsrctext(void)
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

    return srctext;
}

bool compileback(void)
{
    back_castreduction();
    if(emitbackir)
    {
        ir_dump();
        return true;
    }

    reglifetime();
    regalloc();
    if(emitreggraph)
    {
        dumpreggraph();
        return true;
    }

    back_lower();
    if(emitlowered)
    {
        ir_dump();
        return true;
    }

    back_gen();
    
    return false;
}

bool compilemiddle(void)
{
    flow();
    if(emitflow)
    {
        ir_dumpflow();
        return true;
    }
    if(emitdomtree)
    {
        ir_dumpdomtree();
        return true;
    }

    ssa();
    optimize();
    if(emitssa)
    {
        ir_dump();
        return true;
    }

    return false;
}

bool compilefront(void)
{
    lex();
    free(srctext);

    parse();
    semantics();
    if(emitast)
    {
        dumpast();
        front_free();
        return true;
    }
    
    gen();
    if(emitir)
    {
        ir_dump();
        ir_free();
        front_free();
        return true;
    }

    front_free();
    return false;
}

void compile(void)
{
    loadsrctext();

    if(compilefront())
        return;

    if(compilemiddle())
    {
        ir_free();
        return;
    }

    compileback();
    
    ir_free();
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
        if(0);
        else if(!strcmp(argv[i], "-emit-ast"))
            emitast = true;
        else if(!strcmp(argv[i], "-emit-ir"))
            emitir = true;
        else if(!strcmp(argv[i], "-emit-flow"))
            emitflow = true;
        else if(!strcmp(argv[i], "-emit-domtree"))
            emitdomtree = true;
        else if(!strcmp(argv[i], "-emit-ssa"))
            emitssa = true;
        else if(!strcmp(argv[i], "-emit-backir"))
            emitbackir = true;
        else if(!strcmp(argv[i], "-emit-lowered"))
            emitlowered = true;
        else if(!strcmp(argv[i], "-emit-reggraph"))
            emitreggraph = true;
        else
        {
            usage(argv[0]);
            return 1;
        }
    }
    
    back_init();

    for(; i<argc; i++)
    {
        strcpy(srcpath, argv[i]);
        compile();
    }

    return 0;
}
