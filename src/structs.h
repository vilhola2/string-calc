#ifndef STRUCTS_H
#define STRUCTS_H

#include <gmp.h>
#include <mpfr.h>
#include <stdint.h>
#include "defs.h"

typedef enum : uint8_t {
    ADD,
    SUBTRACT,
    NEGATE,
    MULTIPLY,
    DIVIDE,
    LEFT_PARENTHESIS,
    RIGHT_PARENTHESIS,
    SET_VAR,
    EQUALITY,
} OperationType;

typedef struct {
    bool is_digit : 1, is_operator : 1, is_var : 1, is_right_associative : 1;
    int8_t precedence;

    union {
        OperationType operation;
        mpfr_t digits;
        char var;
    };
} Token;

typedef struct {
    Token *arr;
    size_t len;
} TokenArray;

typedef struct {
    Token *items;
    size_t top;
    size_t capacity;
} Stack;

typedef struct {
    mpfr_t var;
    bool is_initialized;
} UserVars;
extern UserVars vars[LETTERS];

#endif

