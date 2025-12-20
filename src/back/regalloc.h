#ifndef _REGALLOC_H
#define _REGALLOC_H

#include "middle/ir.h"

// default to callee saved
#define HARDREG_CALLER      0x01
#define HARDREG_SCRATCH     0x02
#define HARDREG_PARAM       0x04
#define HARDREG_RETURN      0x10
#define HARDREG_INDIRECTADR 0x20

typedef struct hardreg_s
{
    // HARDREG_XX
    uint32_t flags;
    char *name;
    // ir_prim_e -> str
    map_u64_str_t names;
} hardreg_t;

LIST_DECL(hardreg_t, hardreg)
LIST_DECL(hardreg_t*, phardreg)
SET_DECL(hardreg_t, hardreg)
SET_DECL(hardreg_t*, phardreg)

extern list_hardreg_t regpool;
extern set_phardreg_t calleepool;
extern set_phardreg_t callerpool;
extern set_phardreg_t scratchpool;
extern list_phardreg_t scratchlist;
extern list_phardreg_t parampool;
extern list_phardreg_t retpool;

// call after spec init
void regalloc_init(void);
void reglifetime(void);
void regalloc(void);
void dumpreggraph(void);

#endif
