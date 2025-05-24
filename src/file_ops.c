#include <gmp.h>
#include <mpfr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

UserVars vars[LETTERS] = {0};

void cleanup_vars(void) {
    for (int i = 0; i < LETTERS; ++i) {
        if (vars[i].is_initialized) {
            mpfr_clear(vars[i].var);
        }
    }
}

bool write_all_vars(void) {
    FILE *fp = fopen(".variables", "w");
    if (!fp) {
        fprintf(stderr, "write_all_vars: Failed to open file for writing\n");
        return false;
    }
    for (int8_t i = 0; i < LETTERS; ++i) {
        if (!vars[i].is_initialized) {
            continue;
        }
        char *val_str;
        mpfr_asprintf(&val_str, "%Re", vars[i].var);
        fprintf(fp, "%c=%s\n", 'A' + i, val_str);
        mpfr_free_str(val_str);
    }
    fclose(fp);
    return true;
}

bool read_vars(void) {
    FILE *fp = fopen(".variables", "r");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    if (!sz) return false;
    rewind(fp);
    char *line = malloc(sz + 1); // allocate maximum length
    while (fgets(line, sz, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[1] != '=') continue;
        char var = line[0];
        if (var < 'A' || var > 'Z') continue;
        int i = var - 'A';
        if (vars[i].is_initialized) mpfr_clear(vars[i].var);
        mpfr_init2(vars[i].var, MIN_BITS);
        if (mpfr_set_str(vars[i].var, line + 2, 0, MPFR_RNDN)) {
            fprintf(stderr, "read_vars: Failed to parse value for %c\n", var);
            mpfr_clear(vars[i].var);
            vars[i].is_initialized = false;
        } else {
            vars[i].is_initialized = true;
        }
    }
    free(line);
    fclose(fp);
    return true;
}

