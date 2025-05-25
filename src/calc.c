#include <assert.h>
#include <ctype.h>
#include <gmp.h>
#include <mpfr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "file_ops.h"

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

void print_token_arr(TokenArray *token_arr);

typedef struct {
    Token *items;
    size_t top;
    size_t capacity;
} Stack;

Stack *create_stack(size_t capacity) {
    Stack *stack = malloc(sizeof(Stack));
    if (!stack) return nullptr;
    *stack = (Stack){.items = malloc(sizeof(Token) * capacity), .capacity = capacity};
    if (!stack->items) {
        free(stack);
        return nullptr;
    }
    return stack;
}

void destroy_stack(Stack *stack) {
    if (stack) {
        free(stack->items);
        free(stack);
    }
}

bool stack_push(Stack *stack, Token item) {
    if (stack->top == stack->capacity) return false;
    stack->items[stack->top++] = item;
#ifdef DEBUG
    printf("\nstack_push_debug:\n");
    print_token_arr(&(TokenArray){.arr = &item, 1});
    putchar('\n');
#endif
    return true;
}

bool stack_pop(Stack *stack, Token *item) {
    if (!stack->top) return false;
    *item = stack->items[--stack->top];
#ifdef DEBUG
    printf("\nstack_pop_debug:\n");
    print_token_arr(&(TokenArray){.arr = item, 1});
    putchar('\n');
#endif
    return true;
}

bool stack_peek(const Stack *stack, Token *item) {
    if (!stack->top) return false;
    *item = stack->items[stack->top - 1];
#ifdef DEBUG
    printf("\nstack_peek_debug:\n");
    print_token_arr(&(TokenArray){.arr = item, 1});
    putchar('\n');
#endif
    return true;
}

bool stack_is_empty(const Stack *stack) { return stack->top == 0; }

void str_error_print(const char *error_msg, const char *str, size_t i) {
    fprintf(stderr, "%s\n%s\n", error_msg, str);
    for (size_t j = 0; j < i; ++j)
        putc('-', stderr);
    fprintf(stderr, "^\n");
}

void free_token_array(TokenArray *arr) {
    if (arr->arr) {
        for (size_t i = 0; i < arr->len; ++i)
            if (arr->arr[i].is_digit) mpfr_clear(arr->arr[i].digits);
        free(arr->arr);
    }
}

TokenArray tokenize(const char *str) {
    size_t current_len = strlen(str);
    const size_t starting_len = current_len; // current_len might grow
    TokenArray arr = {.arr = calloc(current_len, sizeof(Token))};
    if (!arr.arr) {
        fprintf(stderr, "tokenize: Calloc failed\n");
        return (TokenArray){0};
    }
    bool expect_operand = true, is_var_assignment = false;
    for (size_t i = 0; i < starting_len; ++i) {
        bool is_float = false;
        // Check for user variable definition first
        if (str[i] >= 'A' && str[i] <= 'Z') {
            arr.arr[arr.len++] = (Token){.is_var = true, .var = str[i]};
            expect_operand = false;
        } else if (arr.len > 0 && str[i] == '=') {
            if (arr.arr[0].is_var && arr.len == 1) {
                arr.arr[arr.len++] = (Token){.is_operator = true, .operation = SET_VAR, .is_right_associative = true};
                is_var_assignment = true;
            } else if (!is_var_assignment) {
                arr.arr[arr.len++] = (Token){.is_operator = true, .operation = EQUALITY};
            } else {
                str_error_print("You can't use assignment and equality in the same expression", str, i);
                free_token_array(&arr);
                return (TokenArray){0};
            }
        } else if (isdigit(str[i])) {
            size_t start = i;
            bool is_digit = true;
            while (i < current_len && is_digit) {
                if (isdigit(str[i])) {
                    ++i;
                } else {
                    switch (str[i]) {
                        case ',':
                            fprintf(stderr, "Unexpected comma: use '.' instead\n");
                            free_token_array(&arr);
                            return (TokenArray){0};
                        case '.':
                            if (!is_float) {
                                is_float = true;
                                ++i;
                            } else {
                                str_error_print("Unexpected dot", str, i);
                                free_token_array(&arr);
                                return (TokenArray){0};
                            }
                            break;
                        default: {
                            is_digit = false;
                        }
                    }
                }
            }
            size_t digits = i - start;
            char *num = malloc(digits + 1);
            if (!num) {
                fprintf(stderr, "tokenize: Malloc failed\n");
                free_token_array(&arr);
                return (TokenArray){0};
            }
            memcpy(num, &(str[start]), digits);
            num[digits] = '\0';
            arr.arr[arr.len].is_digit = true;
            mpfr_init2(arr.arr[arr.len].digits, MIN_BITS);
            mpfr_set_str(arr.arr[arr.len++].digits, num, 0, MPFR_RNDN);
            free(num);
            expect_operand = false;
            --i; // step back so the next outer `for` increment lands on the operator
        } else {
            if (str[i] == '(') {
                if (arr.len > 0
                    && (arr.arr[arr.len - 1].is_digit
                        || (arr.arr[arr.len - 1].is_operator && arr.arr[arr.len - 1].operation == RIGHT_PARENTHESIS))) {
                    Token *new_arr = realloc(arr.arr, ++current_len * sizeof(Token));
                    if (!new_arr) {
                        fprintf(stderr, "tokenize: Realloc failed\n");
                        free_token_array(&arr);
                        return (TokenArray){0};
                    } else {
                        arr.arr = new_arr;
                        arr.arr[arr.len++] = (Token){.is_operator = true, .operation = MULTIPLY, .precedence = 2};
                    }
                }
                arr.arr[arr.len++] = (Token){.is_operator = true, .operation = LEFT_PARENTHESIS};
                expect_operand = true;
            } else if (str[i] == ')') {
                arr.arr[arr.len++] = (Token){.is_operator = true, .operation = RIGHT_PARENTHESIS};
            } else if (expect_operand) {
                if (str[i] == '+') {
                    continue;
                } else if (str[i] == '-') {
                    if (arr.len > 0 && arr.arr[arr.len - 1].is_operator && arr.arr[arr.len - 1].operation == NEGATE) {
                        arr.len--; // Remove the previous NEGATE
                    } else {
                        arr.arr[arr.len++] = (Token){.is_operator = true, .operation = NEGATE, .precedence = 5};
                    }
                    continue;
                } else {
                    str_error_print("Unexpected operator", str, i);
                    free_token_array(&arr);
                    return (TokenArray){0};
                }
            } else {
                Token t = {.is_operator = true};
                switch (str[i]) {
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

bool get_val(mpfr_t **val, Token *t) {
    assert(!(t->is_var && t->is_digit));
    if (t->is_digit) {
        *val = &t->digits;
        return true;
    }
    if (t->is_var) {
        if (!vars[t->var - 'A'].is_initialized) {
            fprintf(stderr, "get_val: Variable '%c' is not defined\n", t->var);
            //mpfr_clear(result.digits);
            return false;
        }
        *val = &vars[t->var - 'A'].var;
        return true;
    }
    fprintf(stderr, "get_val: Invalid token type\n");
    return false;
}

bool result_is_boolean; // i cant be bothered to find a better way

void apply_operator(Stack *output_stack, Token operator) {
    result_is_boolean = false;
    Token operand1, operand2, result = {.is_digit = true};
    mpfr_init2(result.digits, MIN_BITS);
    if (operator.operation == NEGATE) {
        if (stack_pop(output_stack, &operand1)) {
            if (operand1.is_digit) {
                mpfr_neg(result.digits, operand1.digits, MPFR_RNDN);
                mpfr_clear(operand1.digits);
            } else if (operand1.is_var) {
                if (vars[operand1.var - 'A'].is_initialized) {
                    mpfr_neg(result.digits, vars[operand1.var - 'A'].var, MPFR_RNDN);
                } else {
                    fprintf(stderr, "apply_operator 1: Variable '%c' is not defined\n", operand1.var);
                    mpfr_clear(result.digits);
                    return;
                }
            }
        }
        stack_push(output_stack, result);
        return;
    }
    if (!stack_pop(output_stack, &operand2) || !stack_pop(output_stack, &operand1)) {
        mpfr_clear(result.digits);
        return;
    }
    if (operator.operation == SET_VAR) {
        if (!vars[operand1.var - 'A'].is_initialized) {
            mpfr_init2(vars[operand1.var - 'A'].var, MIN_BITS);
            vars[operand1.var - 'A'].is_initialized = true;
        }
        if (operand2.is_digit) {
            mpfr_set(vars[operand1.var - 'A'].var, operand2.digits, MPFR_RNDN);
            mpfr_set(result.digits, operand2.digits, MPFR_RNDN);
            if (operand2.is_digit) {
                mpfr_clear(operand2.digits);
            }
            stack_push(output_stack, result);
            write_all_vars();
            return;
        } else if (operand2.is_var) {
            if (!vars[operand2.var - 'A'].is_initialized) {
                fprintf(stderr, "apply_operator 2: Variable '%c' is not defined\n", operand2.var);
                mpfr_clear(result.digits);
                return;
            }
            mpfr_set(vars[operand1.var - 'A'].var, vars[operand2.var - 'A'].var, MPFR_RNDN);
            mpfr_set(result.digits, vars[operand2.var - 'A'].var, MPFR_RNDN);
            stack_push(output_stack, result);
            return;
        }
    }
    mpfr_t *val1, *val2;
    bool init_val1 = false, init_val2 = false;
    if (get_val(&val1, &operand1)) {
        init_val1 = true;
    }
    if (get_val(&val2, &operand2)) {
        init_val2 = true;
    }
    if (!init_val1 || !init_val2) {
        goto cleanup;
    }
    bool success = true;
    switch (operator.operation) {
        case ADD: mpfr_add(result.digits, *val1, *val2, MPFR_RNDN); break;
        case SUBTRACT: mpfr_sub(result.digits, *val1, *val2, MPFR_RNDN); break;
        case MULTIPLY: mpfr_mul(result.digits, *val1, *val2, MPFR_RNDN); break;
        case DIVIDE:
            if (mpfr_zero_p(*val2)) {
                fprintf(stderr, "Error: Division by zero\n");
                success = false;
            } else {
                mpfr_div(result.digits, *val1, *val2, MPFR_RNDN);
            }
            break;
        case EQUALITY:
            if (mpfr_cmp(*val1, *val2) == 0)
                mpfr_set_ui(result.digits, 1, MPFR_RNDN);
            else
                mpfr_set_zero(result.digits, 1);
            result_is_boolean = true;
            break;
        default: success = false; break;
    }
    if (success) {
        stack_push(output_stack, result);
    } else {
        mpfr_clear(result.digits);
    }
cleanup:
    if (operand1.is_digit) {
        mpfr_clear(operand1.digits);
    }
    if (operand2.is_digit) {
        mpfr_clear(operand2.digits);
    }
}

const char *calculate_infix(const char *expression) {
    TokenArray tokens = tokenize(expression);
    if (!tokens.arr) return nullptr;
#ifdef DEBUG
    print_token_arr(&tokens);
#endif
    Stack *operator_stack = create_stack(tokens.len);
    Stack *output_stack = create_stack(tokens.len);
    if (!operator_stack || !output_stack) {
        free_token_array(&tokens);
        if (operator_stack) destroy_stack(operator_stack);
        if (output_stack) destroy_stack(output_stack);
        return nullptr;
    }
    // Process tokens using Shunting Yard algorithm
    for (size_t i = 0; i < tokens.len; i++) {
        Token current = tokens.arr[i];
        if (current.is_operator && current.operation == LEFT_PARENTHESIS) {
            stack_push(operator_stack, current);
        } else if (current.is_operator && current.operation == RIGHT_PARENTHESIS) {
            Token top_op;
            while (!stack_is_empty(operator_stack)) {
                stack_pop(operator_stack, &top_op);
                if (top_op.is_operator && top_op.operation == LEFT_PARENTHESIS) {
                    break;
                }
                apply_operator(output_stack, top_op);
            }
        } else if (current.is_var) {
            stack_push(output_stack, current);
        } else if (current.is_digit) {
            Token new_token = {
                .is_digit = true,
            };
            mpfr_init2(new_token.digits, MIN_BITS);
            mpfr_set(new_token.digits, current.digits, MPFR_RNDN);
            stack_push(output_stack, new_token);
        } else {
            Token top_op;
            while (!stack_is_empty(operator_stack) && stack_peek(operator_stack, &top_op) && (top_op.is_operator)
                   && ((top_op.precedence > current.precedence)
                       || (top_op.precedence == current.precedence && !current.is_right_associative))) {
                if (!stack_pop(operator_stack, &top_op)) {
                    fprintf(stderr, "Failed to pop operator\n");
                    break;
                }
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

    Token final_result;
    char *result_str = nullptr;
    if (stack_pop(output_stack, &final_result) && stack_is_empty(output_stack)) {
        if (result_is_boolean) {
            if (mpfr_cmp_ui(final_result.digits, 1) == 0)
                result_str = "true";
            else
                result_str = "false";
            mpfr_clear(final_result.digits);
        } else if (final_result.is_digit) {
            mpfr_asprintf(&result_str, "%Rg", final_result.digits);
            mpfr_clear(final_result.digits);
        } else if (final_result.is_var) {
            if (vars[final_result.var - 'A'].is_initialized) {
                mpfr_asprintf(&result_str, "%Rg", vars[final_result.var - 'A'].var);
            } else {
                fprintf(stderr, "calculate_infix: Variable '%c' is not defined\n", final_result.var);
            }
        }
    } else {
        // Cleanup
        while (stack_pop(output_stack, &final_result)) {
            if (final_result.is_digit) {
                mpfr_clear(final_result.digits);
            }
        }
    }
    destroy_stack(operator_stack);
    destroy_stack(output_stack);
    free_token_array(&tokens);
    return result_str;
}

