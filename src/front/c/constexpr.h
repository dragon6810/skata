#ifndef _CONSTEXPR_H
#define _CONSTEXPR_H

#include <stdint.h>

#include "ast.h"

int64_t constexpr_eval(const expr_t* expr);

#endif