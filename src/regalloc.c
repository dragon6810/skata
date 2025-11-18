#include "regalloc.h"

int nreg = 16;

void regalloc_funcdef(ir_funcdef_t* funcdef)
{

}

void regalloc(void)
{
    int i;

    for(i=0; i<ir.defs.len; i++)
        regalloc_funcdef(&ir.defs.data[i]);
}

