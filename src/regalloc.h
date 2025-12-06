#ifndef _REGALLOC_H
#define _REGALLOC_H

#include "ir.h"

// default to callee saved
#define HARDREG_CALLER      0x01
#define HARDREG_SCRATCH     0x02
#define HARDREG_PARAM       0x04
#define HARDREG_RETURN      0x10
#define HARDREG_INDIRECTADR 0x20

typedef struct hardreg_s
{
    uint32_t index;
    // HARDREG_XX
    uint32_t flags;
    char *name;
} hardreg_t;

LIST_DECL(hardreg_t, hardreg)
SET_DECL(hardreg_t, hardreg)
SET_DECL(hardreg_t*, phardreg)

extern list_hardreg_t regpool;

void reglifetime(void);
void regalloc(void);
void dumpreggraph(void);

#endif
