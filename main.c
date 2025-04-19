#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include <mpfr.h>

#define MIN_BITS 256

typedef struct {
    bool is_digit;
    bool is_operator;
    int8_t precedence;
    union {
        enum {
            ADD,
            SUBTRACT,
            NEGATE,
            MULTIPLY,
            DIVIDE,
        } operation;
        char *digits;
    };
} Token;

typedef struct {
    Token *arr;
    size_t len;
} TokenArray;

void str_error_print(const char *error_msg, const char *str, size_t i) {
    fprintf(stderr, "%s\n%s\n", error_msg, str);
    for(size_t j = 0; j < i; ++j) putc('-', stderr);
    fprintf(stderr, "^\n");
}

void free_token_array(TokenArray *arr) {
    if(arr->arr) {
        for(size_t i = 0; i < arr->len; ++i) if(arr->arr[i].is_digit) free(arr->arr[i].digits);
        free(arr->arr);
    }
}

TokenArray tokenize(const char *str) {
    size_t len = strlen(str);
    TokenArray arr = { .arr = calloc(len, sizeof(Token)) };
    if(!arr.arr) {
        fprintf(stderr, "tokenize: Calloc failed\n");
        return (TokenArray){0};
    }
    bool expect_operand = true;
    for(size_t i = 0; i < len; ++i) {
        bool is_float = false;
        if(isdigit(str[i])) {
            size_t start = i;
            bool is_digit = true;
            while(i < len && is_digit) {
                if(isdigit(str[i])) ++i;
                else switch(str[i]) {
                    case '.': case ',':
                        if(!is_float) {
                            is_float = true;
                            ++i;
                        } else {
                            str_error_print("Unexpected dot", str, i);
                            free_token_array(&arr);
                            return (TokenArray){0};
                        }
                        break;
                    default: { is_digit = false; }
                }
            }
            size_t digits = i - start;
            char *num = malloc(digits + 1);
            if(!num) {
                fprintf(stderr, "tokenizer: Malloc failed\n");
                free_token_array(&arr);
                return (TokenArray){0};
            }
            memcpy(num, &(str[start]), digits);
            num[digits] = '\0';
            arr.arr[arr.len++] = (Token){ .is_digit = true, .digits = num };
            expect_operand = false;
            --i;  // step back so the next outer `for` increment lands on the operator
        } else {
            if (expect_operand) {
                if (str[i] == '+') continue;
                else if (str[i] == '-') {
                    Token t = { .is_operator = true, .operation = NEGATE };
                    arr.arr[arr.len++] = t;
                    continue;
                }
                else {
                    str_error_print("Unexpected operator", str, i);
                    free_token_array(&arr);
                    return (TokenArray){0};
                }
            }
            else {
                Token t = {.is_operator = true};
                switch (str[i]) {
                    case '+': t.operation = ADD; break;
                    case '-': t.operation = SUBTRACT; break;
                    case '*': t.operation = MULTIPLY; break;
                    case '/': t.operation = DIVIDE; break;
                    default:
                        str_error_print("Invalid syntax", str, i);
                        free_token_array(&arr);
                        return (TokenArray){0};
                }
                arr.arr[arr.len++] = t;
                expect_operand = true;
            }
        }
    }
    return arr;
}

bool shunting_yard(mpfr_t result, char *str) {
    (void) result;
    (void) str;
    TokenArray token_arr = tokenize(str);
    if(!token_arr.arr) return false;
cleanup:
    free_token_array(&token_arr);
    return true;
    goto cleanup;
}

char *str_input(FILE *stream) {
    int ch;
    size_t len = 0;
    size_t bufsize = 16;
    char *buffer = malloc(bufsize);
    if(!buffer) {
        printf("str_input: Malloc failed\n");
        return NULL;
    }
    while((ch = fgetc(stream)) != EOF && ch != '\n') {
        buffer[len++] = ch;
        if(len + 1 == bufsize) {
            char *temp = realloc(buffer, (bufsize += 16));
            if(!temp) {
                printf("str_input: Realloc failed\n");
                free(buffer);
                return NULL;
            } else {
                buffer = temp;
            }
        }
    }
    buffer[len] = '\0';
    return buffer;
}

int main(void) {
    char *expression = str_input(stdin);
    {
        size_t sz = strlen(expression);
        for(size_t i = 0; i < sz; ++i) if(expression[i] == ' ') memmove(expression + i, expression + i + 1, sz - i);
    }
    mpfr_t result;
    mpfr_init2(result, MIN_BITS);
    bool success = shunting_yard(result, expression);
    mpfr_clear(result);
    if(success) return EXIT_SUCCESS;
    else return EXIT_FAILURE;
}
