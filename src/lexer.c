#include <gmp.h>
#include <mpfr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "structs.h"

void free_token_array(TokenArray *arr) {
    if (arr->arr) {
        for (size_t i = 0; i < arr->len; ++i) {
            if (arr->arr[i].is_digit) mpfr_clear(arr->arr[i].digits);
        }
        free(arr->arr);
    }
}

// Debug function
void print_token_arr(TokenArray *token_arr) {
    for (size_t i = 0; i < token_arr->len; ++i) {
        if (token_arr->arr[i].is_digit) {
            mpfr_printf("Number: %Rg\n", token_arr->arr[i].digits);
            continue;
        }
        if (token_arr->arr[i].is_var) {
            printf("Variable: %c\n", token_arr->arr[i].var);
            continue;
        }
        printf("Operation: ");
        if (token_arr->arr[i].operation == ADD)
            printf("ADD");
        else if (token_arr->arr[i].operation == NEGATE)
            printf("NEGATE");
        else if (token_arr->arr[i].operation == SUBTRACT)
            printf("SUBTRACT");
        else if (token_arr->arr[i].operation == DIVIDE)
            printf("DIVIDE");
        else if (token_arr->arr[i].operation == MULTIPLY)
            printf("MULTIPLY");
        else if (token_arr->arr[i].operation == LEFT_PARENTHESIS)
            printf("LEFT_PAREN");
        else if (token_arr->arr[i].operation == RIGHT_PARENTHESIS)
            printf("RIGHT_PAREN");
        else if (token_arr->arr[i].operation == SET_VAR)
            printf("SET_VAR");
        else if (token_arr->arr[i].operation == EQUALITY)
            printf("EQUALITY");
        printf(", precedence: %d\n", token_arr->arr[i].precedence);
    }
}

void lexer_print_error(const char *error_msg, const char *str, size_t i) {
    fprintf(stderr, "%s\n%s\n", error_msg, str);
    for (size_t j = 0; j < i; ++j)
        putc('-', stderr);
    fprintf(stderr, "^\n");
}

typedef struct {
    TokenArray arr;
    const char *str;
    size_t current_len;
    bool expect_operand : 1, is_var_assignment : 1;
} LexerData;

typedef enum {
    LEXER_OK,
    LEXER_SKIP,
    LEXER_ERROR,
} LexerResult;

LexerResult lexer_handle_variable(LexerData *data, size_t i) {
    if (data->str[i] >= 'A' && data->str[i] <= 'Z') {
        data->arr.arr[data->arr.len++] = (Token){.is_var = true, .var = data->str[i]};
        data->expect_operand = false;
        return LEXER_SKIP;
    } else if (data->arr.len > 0 && data->str[i] == '=') {
        if (data->arr.arr[0].is_var && data->arr.len == 1) {
            data->arr.arr[data->arr.len++] = (Token){.is_operator = true, .operation = SET_VAR, .is_right_associative = true};
            data->is_var_assignment = true;
            return LEXER_SKIP;
        } else if (!data->is_var_assignment) {
            data->arr.arr[data->arr.len++] = (Token){.is_operator = true, .operation = EQUALITY};
            return LEXER_SKIP;
        } else {
            lexer_print_error("You can't use assignment and equality in the same expression", data->str, i);
            free_token_array(&data->arr);
            return LEXER_ERROR;
        }
    }
    return LEXER_OK;
}

LexerResult lexer_handle_number(LexerData *data, size_t *i) {
    if (!isdigit(data->str[*i])) return LEXER_OK;
    size_t start = *i;
    bool is_digit = true, is_float = false;
    while (*i < data->current_len && is_digit) {
        if (isdigit(data->str[*i])) ++(*i);
        else switch (data->str[*i]) {
            case ',':
                fprintf(stderr, "Unexpected comma: use '.' instead\n");
                free_token_array(&data->arr);
                return LEXER_ERROR;
            case '.':
                if (!is_float) {
                    is_float = true;
                    ++(*i);
                } else {
                    lexer_print_error("Unexpected dot", data->str, *i);
                    free_token_array(&data->arr);
                    return LEXER_ERROR;
                }
                break;
            default: is_digit = false;
        }
    }
    size_t digits = *i - start;
    char *num = malloc(digits + 1);
    if (!num) {
        fprintf(stderr, "lexer_handle_number: Malloc failed\n");
        free_token_array(&data->arr);
        return LEXER_ERROR;
    }
    memcpy(num, &(data->str[start]), digits);
    num[digits] = '\0';
    data->arr.arr[data->arr.len].is_digit = true;
    mpfr_init2(data->arr.arr[data->arr.len].digits, MIN_BITS);
    mpfr_set_str(data->arr.arr[data->arr.len++].digits, num, 0, MPFR_RNDN);
    free(num);
    data->expect_operand = false;
    --(*i); // step back so the next outer `for` increment lands on the operator
    return LEXER_SKIP;
}

LexerResult lexer_handle_parenthesis(LexerData *data, size_t i) {
    if (data->str[i] == '(') {
        if (data->arr.len > 0
            && (data->arr.arr[data->arr.len - 1].is_digit
                || (data->arr.arr[data->arr.len - 1].is_operator && data->arr.arr[data->arr.len - 1].operation == RIGHT_PARENTHESIS))) {
            Token *new_arr = realloc(data->arr.arr, ++data->current_len * sizeof(Token));
            if (!new_arr) {
                fprintf(stderr, "lexer_handle_parenthesis: Realloc failed\n");
                free_token_array(&data->arr);
                return LEXER_ERROR;
            } else {
                data->arr.arr = new_arr;
                data->arr.arr[data->arr.len++] = (Token){.is_operator = true, .operation = MULTIPLY, .precedence = 2};
            }
        }
        data->arr.arr[data->arr.len++] = (Token){.is_operator = true, .operation = LEFT_PARENTHESIS};
        data->expect_operand = true;
        return LEXER_SKIP;
    } else if (data->str[i] == ')') {
        data->arr.arr[data->arr.len++] = (Token){.is_operator = true, .operation = RIGHT_PARENTHESIS};
        return LEXER_SKIP;
    }
    return LEXER_OK;
}

LexerResult lexer_handle_operator(LexerData *data, size_t i) {
    if (data->expect_operand) {
        if (data->str[i] == '+') return LEXER_SKIP;
        else if (data->str[i] == '-') {
            if (data->arr.len > 0
                && data->arr.arr[data->arr.len - 1].is_operator
                && data->arr.arr[data->arr.len - 1].operation == NEGATE) {
                data->arr.len--; // Remove the previous NEGATE
            } else data->arr.arr[data->arr.len++] = (Token){.is_operator = true, .operation = NEGATE, .precedence = 5};
            return LEXER_SKIP;
        } else {
            lexer_print_error("Unexpected operator", data->str, i);
            free_token_array(&data->arr);
            return LEXER_ERROR;
        }
    } else {
        Token t = {.is_operator = true};
        switch (data->str[i]) {
            case '+':
                t.operation = ADD;
                t.precedence = 1;
                break;
            case '-':
                t.operation = SUBTRACT;
                t.precedence = 1;
                break;
            case '*':
                t.operation = MULTIPLY;
                t.precedence = 2;
                break;
            case '/':
                t.operation = DIVIDE;
                t.precedence = 2;
                break;
            default:
                lexer_print_error("Invalid syntax", data->str, i);
                free_token_array(&data->arr);
                return LEXER_ERROR;
        }
        data->arr.arr[data->arr.len++] = t;
        data->expect_operand = true;
        return LEXER_SKIP;
    }
}

TokenArray tokenize(const char *str) {
    LexerData data = {
        .str = str,
        .current_len = strlen(str),
        .expect_operand = true,
        .is_var_assignment = false,
    };
    if (!(data.arr.arr = calloc(data.current_len, sizeof(Token)))) {
        fprintf(stderr, "tokenize: Calloc failed\n");
        return (TokenArray){0};
    }
    const size_t starting_len = data.current_len; // current_len might grow

    LexerResult result;
    for (size_t i = 0; i < starting_len; ++i) {
        result = lexer_handle_variable(&data, i);
        if (result == LEXER_SKIP) continue;
        if (result == LEXER_ERROR) return (TokenArray){0};

        result = lexer_handle_number(&data, &i);
        if (result == LEXER_SKIP) continue;
        if (result == LEXER_ERROR) return (TokenArray){0};

        result = lexer_handle_parenthesis(&data, i);
        if (result == LEXER_SKIP) continue;
        if (result == LEXER_ERROR) return (TokenArray){0};

        result = lexer_handle_operator(&data, i);
        if (result == LEXER_SKIP) continue;
        if (result == LEXER_ERROR) return (TokenArray){0};
    }
    return data.arr;
}
