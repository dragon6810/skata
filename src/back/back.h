#ifndef _BACK_H
#define _BACK_H

#include "middle/ir.h"
#include "map.h"

extern map_u64_u64_t typereductions;

void back_init(void);
// reduce types, e.g. u1 -> u8 or ptr -> u64
void back_typereduction(void);
// can you use indirect adressing for this operand?
bool back_canindirect(ir_inst_e opcode, int ioperand);
// insert FIE instructions when a ptr to a stack var is used directly
void back_fie(void);
void back_lower(void);
void back_gen(void);

#endif