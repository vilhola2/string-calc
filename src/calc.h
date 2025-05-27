#ifndef CALC_H
#define CALC_H

#include <gmp.h>
#include <mpfr.h>
#include "file_ops.h"

typedef enum {
    CALC_ERROR = 0,
    CALC_BOOLEAN_STRING,
    CALC_MPFR_STRING,
} CalculatorResultType;

typedef struct {
    CalculatorResultType type;
    char *str;
} CalculatorResult;

// CAUTION! If the result type is MPFR_STRING, 'str' must be freed with mpfr_free_str()!
CalculatorResult calculate_infix(const char *expression);

#endif
