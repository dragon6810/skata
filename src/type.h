#ifndef _TYPE_H
#define _TYPE_H

#include "list.h"

extern list_string_t types;

void type_init(void);
int type_register(const char* name);
// if the type doesn't exist, returns -1
int type_find(const char* name);
void type_free(void);

#endif