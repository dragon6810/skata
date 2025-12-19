#ifndef _ERROR_H
#define _ERROR_H

#include <stdarg.h>
#include <stdbool.h>

#define MAX_ERROR 20

extern int errorcount;

void verror(bool fatal, int line, int col, char* fmt, va_list args);
void error(bool fatal, int line, int col, char* fmt, ...);
void failiferr(void);

#endif