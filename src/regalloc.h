#ifndef _REGALLOC_H
#define _REGALLOC_H

#include "ir.h"

extern int nreg;

void reglifetime(void);
void regalloc(void);
void dumpreggraph(void);

#endif
