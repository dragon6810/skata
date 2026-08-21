#ifndef _CONSTEXPR_H
#define _CONSTEXPR_H

#include <stdint.h>

#include "ast.h"

int64_t intconstexpr_eval(const expr_t* expr);
// will return an expression that is either and atom or a sum of an adress and integer
expr_t* constexpr_eval(const expr_t* expr);

#endif