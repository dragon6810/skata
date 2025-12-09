#include "error.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "flags.h"

int errorcount = 0;

static void println(int line, int col)
{
    int i;

    char *str;
    FILE *ptr;
    int curline;
    size_t linelen;

    printf("%5d | ", line + 1);

    ptr = fopen(srcpath, "r");
    assert(ptr);

    str = NULL;
    curline = 0;
    while (curline < line && (str = fgetln(ptr, &linelen)))
        curline++;

    assert(curline == line);
    str = fgetln(ptr, &linelen);
    assert(str);
    assert(col < linelen);

    for(i=0; i<linelen; i++)
    {
        if(str[i] == '\t')
            printf("    ");
        else
            printf("%c", str[i]);
    }

    printf("      | ");
    for(i=0; i<col; i++)
    {
        if(str[i] == '\t')
            printf("    ");
        else
            printf(" ");
    }
    printf("\e[0;92m^\e[0m\n");

    fclose(ptr);
}

void verror(bool fatal, int line, int col, char* fmt, va_list args)
{
    printf("\e[1m%s:%d:%d: \e[1;31m%serror: \e[0;1m", srcpath, line+1, col+1, fatal ? "fatal " : "");
    vprintf(fmt, args);
    printf("\e[0m");

    println(line, col);

    errorcount++;

    if(fatal)
        exit(EXIT_FAILURE);
    if(errorcount >= MAX_ERROR)
    {
        printf("maximum errors reached, aborting.\n");
        exit(EXIT_FAILURE);
    }
}

void error(bool fatal, int line, int col, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    verror(fatal, line, col, fmt, args);

    va_end(args);
}

void failiferr(void)
{
    if(!errorcount)
        return;
    
    printf("%d error%s generated.\n", errorcount, errorcount != 1 ? "s" : "");
    exit(EXIT_FAILURE);
}
