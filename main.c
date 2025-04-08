#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include <mpfr.h>

#define MIN_BITS 256

int count_significant_digits(const char *str) {
    while(*str == '0') ++str;
    char *dot_pos = strchr(str, '.');
    if(dot_pos) return (dot_pos - str) + strlen(dot_pos + 1);
    else return strlen(str);
}

void calculate(mpfr_t result, char *operand1, char *operand2, int operator) {
    int required_bits;
    {
        int d1 = count_significant_digits(operand1);
        int d2 = count_significant_digits(operand2);
        required_bits = MIN_BITS + (int)ceil(fmax(d1, d2) * log2(10));
    }

    mpfr_t num1, num2;
    mpfr_init2(num1, required_bits);
    mpfr_init2(num2, required_bits);
    mpfr_init2(result, required_bits);
    mpfr_set_str(num1, operand1, 10, MPFR_RNDN);
    mpfr_set_str(num2, operand2, 10, MPFR_RNDN);

    switch(operator) {
        case '+':
            mpfr_add(result, num1, num2, MPFR_RNDN);
            break;
        case '-':
            mpfr_sub(result, num1, num2, MPFR_RNDN);
            break;
        case '*':
            mpfr_mul(result, num1, num2, MPFR_RNDN);
            break;
        case '/':
            if(mpfr_zero_p(num2)) mpfr_set_nan(result);
            else mpfr_div(result, num1, num2, MPFR_RNDN);
            break;
    }
    mpfr_clear(num1);
    mpfr_clear(num2);
}

void capture_operand(char **operand, char *str, size_t start, size_t length) {
    *operand = malloc(length + 1);
    strncpy(*operand, &str[start], length);
    (*operand)[length] = '\0';
}

void parse_calc_str(mpfr_t result, char *str) {
    char *operand1 = {0}, *operand2 = {0};
    char **current_operand = &operand1;
    int saved_operator = {0};
    int32_t len = 0;
    const size_t sz = strlen(str);
    for(size_t i = 0; i < sz; ++i) {
        int ch = str[i];
        if(ch >= '0' && ch <= '9') {
            ++len;
        } else if(ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            saved_operator = ch;
            if (len > 0) {
                capture_operand(current_operand, str, i - len, len);
                len = 0;
                current_operand = (current_operand == &operand1) ? &operand2 : &operand1;
            }
        } else {
            printf("Error: invalid syntax\n%s\n", str);
            for(size_t j = 0; j < i; ++j) putchar('-');
            printf("^\n");
            goto cleanup;
        }
    }
    if (len > 0) {
        capture_operand(current_operand, str, sz - len, len);
    }
    if (operand1 && operand2 && saved_operator) {
        calculate(result, operand1, operand2, saved_operator);
    } else {
        printf("Error: incomplete expression\n");
    }
    cleanup:
    if(operand1) free(operand1);
    if(operand2) free(operand2);
}

int main(int argc, char **argv) {
    if(argc <= 1) {
        printf("Error: No input\n"); 
        return 1;
    }
    char *str = calloc(sizeof(char), strlen(argv[1]) + 1);
    for(int i = 1; i < argc; ++i) {
        const size_t sz_next = strlen(argv[i]);
        const size_t sz_curr = strlen(str);
        char *temp = realloc(str, sz_curr + sz_next + 1);
        if(!temp) {
            free(str);
            return 1;
        } else {
            str = temp;
            strcpy(str + sz_curr, argv[i]);
        }
    }
    mpfr_t result;
    parse_calc_str(result, str);
    free(str);
    mpfr_prec_t prec_bits = mpfr_get_prec(result);
    int dec_digits = (int)ceil(prec_bits * log10(2));
    mpfr_printf("%.*Rg\n", (dec_digits < 15) ? dec_digits : 15, result);
    mpfr_clear(result);
    return 0;
}
