#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asmgen.h"
#include "ast.h"
#include "back.h"
#include "flags.h"
#include "ir.h"
#include "map.h"
#include "regalloc.h"
#include "semantics.h"
#include "token.h"
#include "type.h"

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

void compile(void)
{
    loadsrctext();

    lex();
    free(srctext);

    parse();
    semantics();
    if(emitast)
    {
        dumpast();

        list_globaldecl_free(&ast);
        list_token_free(&tokens);
        return;
    }
    
    ir_gen();
    if(emitir)
    {
        ir_dump();
        goto freestuff;
    }

    ir_flow();
    if(emitflow)
    {
        ir_dumpflow();
        goto freestuff;
    }
    if(emitdomtree)
    {
        ir_dumpdomtree();
        goto freestuff;
    }

    ir_ssa();
    ir_middleoptimize();
    if(emitssa)
    {
        ir_dump();
        goto freestuff;
    }

    back_castreduction();
    ir_middleoptimize();
    if(emitbackir)
    {
        ir_dump();
        goto freestuff;
    }

    reglifetime();
    regalloc();
    if(emitreggraph)
    {
        dumpreggraph();
        goto freestuff;
    }

    ir_lower();
    // ir_backoptimize();
    if(emitlowered)
    {
        ir_dump();
        goto freestuff;
    }

    asmgen_arm();
    
freestuff:
    ir_free();
    list_token_free(&tokens);
    list_globaldecl_free(&ast);
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
    
    arm_specinit();
    regalloc_init();

    for(; i<argc; i++)
    {
        strcpy(srcpath, argv[i]);
        compile();
    }

    return 0;
}
