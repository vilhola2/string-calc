#ifndef CALC_H
#define CALC_H

#include <gmp.h>
#include <mpfr.h>
#include "file_ops.h"

// WARNING! Result must be freed with mpfr_free_str()!
char *calculate_infix(const char *expression);

#endif
