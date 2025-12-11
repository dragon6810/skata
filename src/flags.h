#ifndef _FLAGS_H
#define _FLAGS_H

#include <stdbool.h>
#include <limits.h>

extern bool emitast;
extern bool emitpostsem;
extern bool emitir;
extern bool emitflow;
extern bool emitdomtree;
extern bool emitssa;
extern bool emitlowered;
extern bool emitreggraph;

extern char srcpath[PATH_MAX];

#endif