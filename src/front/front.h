#ifndef _FRONT_H
#define _FRONT_H

extern char *srctext;

void lex(void);
void parse(void);
void semantics(void);
void gen(void);

void dumpast();

void front_free(void);

#endif