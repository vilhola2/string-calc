#ifndef CALC_H
#define CALC_H

#include <gmp.h>
#include <mpfr.h>

bool calculate_infix(mpfr_t result, const char *expression);

#endif
