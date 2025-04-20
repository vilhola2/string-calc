#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <gmp.h>
#include <mpfr.h>
#include "defs.h"

extern struct {
    mpfr_t var;
    bool is_initialized;
} vars[LETTERS];

void cleanup_vars(void);
bool write_all_vars(void);
bool read_vars(void);

#endif
