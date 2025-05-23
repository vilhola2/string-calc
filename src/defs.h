#ifndef DEFS_H
#define DEFS_H

#include <gmp.h>
#include <mpfr.h>

#define MIN_BITS 256
#define LETTERS 26

typedef struct {
    mpfr_t var;
    bool is_initialized;
} UserVars;
extern UserVars vars[LETTERS];

#endif
