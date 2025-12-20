#ifndef _BACK_H
#define _BACK_H

#include "map.h"

extern map_u64_u64_t typereductions;

void back_init(void);
// reduce types, e.g. u1 -> u8 or ptr -> u64
void back_typereduction(void);
void back_lower(void);
void back_gen(void);

#endif