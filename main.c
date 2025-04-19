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
        mpfr_t digits;
    };
} Token;

typedef struct {
    Token *arr;
    size_t len;
} TokenArray;

typedef struct {
    Token *items;
    int top;
    size_t capacity;
} Stack;

Stack* create_stack(size_t capacity) {
    Stack *stack = malloc(sizeof(Stack));
    if (!stack) return NULL;

    stack->items = malloc(sizeof(Token) * capacity);
    if (!stack->items) {
        free(stack);
        return NULL;
    }

    stack->capacity = capacity;
    stack->top = -1;
    return stack;
}

void destroy_stack(Stack *stack) {
    if (stack) {
        free(stack->items);
        free(stack);
    }
}

bool stack_push(Stack *stack, Token item) {
    if (stack->top >= (int)stack->capacity - 1) return false;
    stack->items[++stack->top] = item;
    return true;
}

bool stack_pop(Stack *stack, Token *item) {
    if (stack->top < 0) return false;
    *item = stack->items[stack->top--];
    return true;
}

bool stack_peek(const Stack *stack, Token *item) {
    if (stack->top < 0) return false;
    *item = stack->items[stack->top];
    return true;
}

bool stack_is_empty(const Stack *stack) {
    return stack->top < 0;
}

void str_error_print(const char *error_msg, const char *str, size_t i) {
    fprintf(stderr, "%s\n%s\n", error_msg, str);
    for(size_t j = 0; j < i; ++j) putc('-', stderr);
    fprintf(stderr, "^\n");
}

void free_token_array(TokenArray *arr) {
    if(arr->arr) {
        for(size_t i = 0; i < arr->len; ++i) if(arr->arr[i].is_digit) mpfr_clear(arr->arr[i].digits);
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
            arr.arr[arr.len].is_digit = true;
            mpfr_init2(arr.arr[arr.len].digits, MIN_BITS);
            mpfr_set_str(arr.arr[arr.len++].digits, num, 0, MPFR_RNDN);
            expect_operand = false;
            --i;  // step back so the next outer `for` increment lands on the operator
        } else {
            if (expect_operand) {
                if (str[i] == '+') continue;
                else if (str[i] == '-') {
                    if (arr.len > 0 && arr.arr[arr.len-1].is_operator && arr.arr[arr.len-1].operation == NEGATE) {
                        arr.len--; // Remove the previous NEGATE
                    } else {
                        arr.arr[arr.len++] = (Token){ .is_operator = true, .operation = NEGATE, .precedence = 5 };
                    }
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
                    case '+': t.operation = ADD; t.precedence = 1; break;
                    case '-': t.operation = SUBTRACT; t.precedence = 1; break;
                    case '*': t.operation = MULTIPLY; t.precedence = 2; break;
                    case '/': t.operation = DIVIDE; t.precedence = 2; break;
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

void print_token_arr(TokenArray *token_arr) {
    for(size_t i = 0; i < token_arr->len; ++i) {
        bool is_number = (token_arr->arr[i].is_digit);
        printf("%s ",  (is_number) ? "Number:" : "Operator:");
        if(is_number) {
            mpfr_printf("%Rg\n", token_arr->arr[i].digits);
            continue;
        }
        else if(token_arr->arr[i].operation == ADD) printf("ADD");
        else if(token_arr->arr[i].operation == NEGATE) printf("NEGATE");
        else if(token_arr->arr[i].operation == SUBTRACT) printf("SUBTRACT");
        else if(token_arr->arr[i].operation == DIVIDE) printf("DIVIDE");
        else if(token_arr->arr[i].operation == MULTIPLY) printf("MULTIPLY");
        printf(", precedence: %d\n", token_arr->arr[i].precedence);
    }
}

static void apply_operator(Stack *output_stack, Token operator) {
    Token operand1, operand2, result;

    result.is_digit = true;
    result.is_operator = false;
    mpfr_init2(result.digits, MIN_BITS);

    if (operator.operation == NEGATE) {
        if (stack_pop(output_stack, &operand1)) {
            if (operand1.is_digit) {
                mpfr_neg(result.digits, operand1.digits, MPFR_RNDN);
                mpfr_clear(operand1.digits);
            }
        }
        stack_push(output_stack, result);
        return;
    }

    if (!stack_pop(output_stack, &operand2) || !stack_pop(output_stack, &operand1)) {
        mpfr_clear(result.digits);  // Clear result if we can't get operands
        return;
    }

    if (!operand1.is_digit || !operand2.is_digit) {
        mpfr_clear(result.digits);
        if (operand1.is_digit) mpfr_clear(operand1.digits);
        if (operand2.is_digit) mpfr_clear(operand2.digits);
        return;
    }

    bool success = true;
    switch (operator.operation) {
        case ADD:
            mpfr_add(result.digits, operand1.digits, operand2.digits, MPFR_RNDN);
            break;
        case SUBTRACT:
            mpfr_sub(result.digits, operand1.digits, operand2.digits, MPFR_RNDN);
            break;
        case MULTIPLY:
            mpfr_mul(result.digits, operand1.digits, operand2.digits, MPFR_RNDN);
            break;
        case DIVIDE:
            if (mpfr_zero_p(operand2.digits)) {
                fprintf(stderr, "Error: Division by zero\n");
                success = false;
            } else {
                mpfr_div(result.digits, operand1.digits, operand2.digits, MPFR_RNDN);
            }
            break;
        default: break;
    }

    mpfr_clear(operand1.digits);
    mpfr_clear(operand2.digits);

    if (success) {
        stack_push(output_stack, result);
    } else {
        mpfr_clear(result.digits);
    }
}

bool calculate_infix(mpfr_t result, const char *expression) {
    TokenArray tokens = tokenize(expression);
    if (!tokens.arr) return false;

    Stack *operator_stack = create_stack(tokens.len);
    Stack *output_stack = create_stack(tokens.len);

    if (!operator_stack || !output_stack) {
        free_token_array(&tokens);
        if (operator_stack) destroy_stack(operator_stack);
        if (output_stack) destroy_stack(output_stack);
        return false;
    }

    // Process tokens using Shunting Yard algorithm
    for (size_t i = 0; i < tokens.len; i++) {
        Token current = tokens.arr[i];

        if (current.is_digit) {
            // For digits, we need to create a new token with its own mpfr_t
            Token new_token = {
                .is_digit = true,
                .is_operator = false
            };
            mpfr_init2(new_token.digits, MIN_BITS);
            mpfr_set(new_token.digits, current.digits, MPFR_RNDN);
            stack_push(output_stack, new_token);
        } else {
            Token top_op;
            while (!stack_is_empty(operator_stack) &&
                   stack_peek(operator_stack, &top_op) &&
                   top_op.precedence >= current.precedence) {
                stack_pop(operator_stack, &top_op);
                apply_operator(output_stack, top_op);
            }
            stack_push(operator_stack, current);
        }
    }

    // Process remaining operators
    Token op;
    while (stack_pop(operator_stack, &op)) {
        apply_operator(output_stack, op);
    }

    // Get final result
    Token final_result;
    bool success = false;

    if (stack_pop(output_stack, &final_result) && stack_is_empty(output_stack)) {
        if (final_result.is_digit) {
            mpfr_set(result, final_result.digits, MPFR_RNDN);
            mpfr_clear(final_result.digits);
            success = true;
        }
    }

    // Cleanup any remaining tokens in the stacks
    while (stack_pop(output_stack, &final_result)) {
        if (final_result.is_digit) {
            mpfr_clear(final_result.digits);
        }
    }

    // Cleanup
    destroy_stack(operator_stack);
    destroy_stack(output_stack);
    free_token_array(&tokens);

    return success;
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
    printf("Start typing an expression or enter 'q' to quit\n");
    while(true) {
        char *expression = str_input(stdin);
        if(!strcmp(expression, "q")) return EXIT_SUCCESS;
        {
            size_t sz = strlen(expression);
            for(size_t i = 0; i < sz; ++i) if(expression[i] == ' ') memmove(expression + i, expression + i + 1, sz - i);
        }
        mpfr_t result;
        mpfr_init2(result, MIN_BITS);
        bool success = calculate_infix(result, expression);
        if(success) {
            mpfr_printf("Result: %Rg\n", result);
            mpfr_clear(result);
        } else {
            mpfr_clear(result);
            break;
        }
    }
    return EXIT_FAILURE;
}
