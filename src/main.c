#include <gmp.h>
#include <mpfr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "file_ops.h"

char *str_input(FILE *stream) {
    int ch;
    size_t len = 0;
    size_t bufsize = 16;
    char *buffer = malloc(bufsize);
    if (!buffer) {
        printf("str_input: Malloc failed\n");
        return NULL;
    }
    while ((ch = fgetc(stream)) != EOF && ch != '\n') {
        buffer[len++] = ch;
        if (len + 1 == bufsize) {
            char *temp = realloc(buffer, (bufsize += 16));
            if (!temp) {
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
    read_vars();
    printf("Start typing an expression or enter 'h' for help\n");
    while (true) {
        printf(">  ");
        char *expression = str_input(stdin);
        if (!*(expression)) {
            printf("Empty expression\n");
            free(expression);
            continue;
        }
        if (!strcmp(expression, "h")) {
            printf("You can use parenthesis '()'\nYou can use A-Z as variables\nEnter 'q' to quit\n");
            free(expression);
            continue;
        }
        if (!strcmp(expression, "q")) {
            free(expression);
            write_all_vars();
            cleanup_vars();
            return EXIT_SUCCESS;
        } else {
            // remove the spaces
            size_t read = 0, write = 0;
            while (expression[read] != '\0') {
                if (expression[read++] != ' ') {
                    expression[write++] = expression[read - 1];
                }
            }
            expression[write] = '\0';
        }
        mpfr_t result;
        mpfr_init2(result, 256);
        bool success = calculate_infix(result, expression);
        if (success) {
            mpfr_printf("Result: %Rg\n", result);
            mpfr_clear(result);
            free(expression);
        } else {
            mpfr_clear(result);
            free(expression);
        }
    }
    cleanup_vars();
    return EXIT_FAILURE;
}
