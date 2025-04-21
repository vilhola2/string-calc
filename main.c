#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include <mpfr.h>
#include "defs.h"
#include "file_ops.h"

typedef struct {
    bool is_digit;
    bool is_operator;
    bool is_var;
    bool is_right_associative;
    int8_t precedence;
    union {
        enum {
            ADD,
            SUBTRACT,
            NEGATE,
            MULTIPLY,
            DIVIDE,
            LEFT_PARENTHESIS,
            RIGHT_PARENTHESIS,
            SET_VAR,
        } operation;
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
    int top;
    size_t capacity;
} Stack;

Stack* create_stack(size_t capacity) {
    Stack *stack = malloc(sizeof(Stack));
    if(!stack) return nullptr;
    stack->items = malloc(sizeof(Token) * capacity);
    if(!stack->items) {
        free(stack);
        return nullptr;
    }
    stack->capacity = capacity;
    stack->top = -1;
    return stack;
}

void destroy_stack(Stack *stack) {
    if(stack) {
        free(stack->items);
        free(stack);
    }
}

bool stack_push(Stack *stack, Token item) {
    if(stack->top >= (int)stack->capacity - 1) return false;
    stack->items[++stack->top] = item;
    return true;
}

bool stack_pop(Stack *stack, Token *item) {
    if(stack->top < 0) return false;
    *item = stack->items[stack->top--];
    return true;
}

bool stack_peek(const Stack *stack, Token *item) {
    if(stack->top < 0) return false;
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
    const size_t starting_len = len; // len might grow
    TokenArray arr = { .arr = calloc(len, sizeof(Token)) };
    if(!arr.arr) {
        fprintf(stderr, "tokenize: Calloc failed\n");
        return (TokenArray){0};
    }
    bool expect_operand = true;
    bool has_equals = false;
    bool is_var_assignment = false;
    for(size_t i = 0; i < starting_len; ++i) {
        bool is_float = false;
        // Check for user variable definition first
        if(str[i] >= 'A' && str[i] <= 'Z') {
            arr.arr[arr.len++] = (Token){ .is_var = true, .var = str[i] };
            is_var_assignment = true;
            expect_operand = false;
        } else if(arr.len > 0 && str[i] == '=') {
            if(has_equals) {
                str_error_print("You can't assign twice", str, i);
                free_token_array(&arr);
                return (TokenArray){0};
            }
            if(is_var_assignment) {
                arr.arr[arr.len++] = (Token){ .is_operator = true, .operation = SET_VAR, .is_right_associative = true };
                has_equals = true;
            } else {
                str_error_print("Nothing to assign", str, i);
                free_token_array(&arr);
                return (TokenArray){0};
            }
        }
        else if(isdigit(str[i])) {
            size_t start = i;
            bool is_digit = true;
            while(i < len && is_digit) {
                if(isdigit(str[i])) ++i;
                else switch(str[i]) {
                    case ',':
                        fprintf(stderr, "Unexpected comma: use '.' instead\n");
                        free_token_array(&arr);
                        return (TokenArray){0};
                    case '.':
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
            free(num);
            expect_operand = false;
            --i;  // step back so the next outer `for` increment lands on the operator
        } else {
            if(str[i] == '(') {
                if(arr.len > 0 && (arr.arr[arr.len - 1].is_digit
                    || (arr.arr[arr.len - 1].is_operator
                    && arr.arr[arr.len - 1].operation == RIGHT_PARENTHESIS))
                ) {
                    Token *new_arr = realloc(arr.arr, ++len * sizeof(Token));
                    if(!new_arr) {
                        fprintf(stderr, "tokenize: Realloc failed\n");
                        free_token_array(&arr);
                        return (TokenArray){0};
                    } else {
                        arr.arr = new_arr;
                        arr.arr[arr.len++] = (Token){ .is_operator = true, .operation = MULTIPLY, .precedence = 2 };
                    }
                }
                arr.arr[arr.len++] = (Token){ .is_operator = true, .operation = LEFT_PARENTHESIS };
            }
            else if(str[i] == ')') arr.arr[arr.len++] = (Token){ .is_operator = true, .operation = RIGHT_PARENTHESIS };
            else if(expect_operand) {
                if(str[i] == '+') continue;
                else if(str[i] == '-') {
                    if(arr.len > 0 && arr.arr[arr.len - 1].is_operator && arr.arr[arr.len - 1].operation == NEGATE) {
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
                Token t = { .is_operator = true };
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

// Debug function
void print_token_arr(TokenArray *token_arr) {
    for(size_t i = 0; i < token_arr->len; ++i) {
        if(token_arr->arr[i].is_digit) {
            mpfr_printf("Number: %Rg\n", token_arr->arr[i].digits);
            continue;
        }
        if(token_arr->arr[i].is_var) {
            printf("Variable: %c\n", token_arr->arr[i].var);
            continue;
        }
        printf("Operation: ");
        if(token_arr->arr[i].operation == ADD) printf("ADD");
        else if(token_arr->arr[i].operation == NEGATE) printf("NEGATE");
        else if(token_arr->arr[i].operation == SUBTRACT) printf("SUBTRACT");
        else if(token_arr->arr[i].operation == DIVIDE) printf("DIVIDE");
        else if(token_arr->arr[i].operation == MULTIPLY) printf("MULTIPLY");
        else if(token_arr->arr[i].operation == LEFT_PARENTHESIS) printf("LEFT_PAREN");
        else if(token_arr->arr[i].operation == RIGHT_PARENTHESIS) printf("RIGHT_PAREN");
        else if(token_arr->arr[i].operation == SET_VAR) printf("SET_VAR");
        printf(", precedence: %d\n", token_arr->arr[i].precedence);
    }
}

bool get_val(mpfr_t val, Token t) {
    if(t.is_var) {
        if(!vars[t.var - 'A'].is_initialized) {
            fprintf(stderr, "Variable '%c' is not defined\n", t.var);
            //mpfr_clear(result.digits);
            return false;
        }
        mpfr_init2(val, MIN_BITS);
        mpfr_set(val, vars[t.var - 'A'].var, MPFR_RNDN);
    } else if(t.is_digit) {
        mpfr_init2(val, MIN_BITS);
        mpfr_set(val, t.digits, MPFR_RNDN);
    }
    return true;
}

void apply_operator(Stack *output_stack, Token operator) {
    Token operand1, operand2, result;
    result.is_digit = true;
    result.is_operator = false;
    mpfr_init2(result.digits, MIN_BITS);
    if(operator.operation == NEGATE) {
        if(stack_pop(output_stack, &operand1)) {
            if(operand1.is_digit) {
                mpfr_neg(result.digits, operand1.digits, MPFR_RNDN);
                mpfr_clear(operand1.digits);
            } else if(operand1.is_var) {
                if(vars[operand1.var - 'A'].is_initialized) {
                    mpfr_neg(result.digits, vars[operand1.var - 'A'].var, MPFR_RNDN);
                } else {
                    fprintf(stderr, "Variable '%c' is not defined\n", operand1.var);
                    mpfr_clear(result.digits);
                    return;
                }
            }
        }
        stack_push(output_stack, result);
        return;
    }
    if(!stack_pop(output_stack, &operand2) || !stack_pop(output_stack, &operand1)) {
        mpfr_clear(result.digits);
        return;
    }
    if(operator.operation == SET_VAR) {
        if(!vars[operand1.var - 'A'].is_initialized) {
            mpfr_init2(vars[operand1.var - 'A'].var, MIN_BITS);
            vars[operand1.var - 'A'].is_initialized = true;
        }
        if(operand2.is_digit) {
            mpfr_set(vars[operand1.var - 'A'].var, operand2.digits, MPFR_RNDN);
            mpfr_set(result.digits, operand2.digits, MPFR_RNDN);
            if(operand2.is_digit) mpfr_clear(operand2.digits);
            stack_push(output_stack, result);
            write_all_vars();
            return;
        } else if(operand2.is_var) {
            if(!vars[operand2.var - 'A'].is_initialized) {
                fprintf(stderr, "Variable '%c' is not defined\n", operand2.var);
                mpfr_clear(result.digits);
                return;
            }
            mpfr_set(vars[operand1.var - 'A'].var, vars[operand2.var - 'A'].var, MPFR_RNDN);
            mpfr_set(result.digits, vars[operand2.var - 'A'].var, MPFR_RNDN);
            stack_push(output_stack, result);
            return;
        }
    }
    mpfr_t val1, val2;
    bool init_val1 = false, init_val2 = false;
    if(get_val(val1, operand1)) init_val1 = true;
    if(get_val(val2, operand2)) init_val2 = true;
    if(!init_val1 || !init_val2) goto cleanup;
    bool success = true;
    switch(operator.operation) {
        case ADD:
            mpfr_add(result.digits, val1, val2, MPFR_RNDN);
            break;
        case SUBTRACT:
            mpfr_sub(result.digits, val1, val2, MPFR_RNDN);
            break;
        case MULTIPLY:
            mpfr_mul(result.digits, val1, val2, MPFR_RNDN);
            break;
        case DIVIDE:
            if(mpfr_zero_p(val2)) {
                fprintf(stderr, "Error: Division by zero\n");
                success = false;
            } else {
                mpfr_div(result.digits, val1, val2, MPFR_RNDN);
            }
            break;
        default: success = false; break;
    }
    if(success) stack_push(output_stack, result);
    else mpfr_clear(result.digits);
cleanup:
    if(init_val1) mpfr_clear(val1);
    if(init_val2) mpfr_clear(val2);
    if(operand1.is_digit) mpfr_clear(operand1.digits);
    if(operand2.is_digit) mpfr_clear(operand2.digits);
}

bool calculate_infix(mpfr_t result, const char *expression) {
    TokenArray tokens = tokenize(expression);
    if(!tokens.arr) return false;
    //print_token_arr(&tokens);
    Stack *operator_stack = create_stack(tokens.len);
    Stack *output_stack = create_stack(tokens.len);
    if(!operator_stack || !output_stack) {
        free_token_array(&tokens);
        if(operator_stack) destroy_stack(operator_stack);
        if(output_stack) destroy_stack(output_stack);
        return false;
    }
    // Process tokens using Shunting Yard algorithm
    for(size_t i = 0; i < tokens.len; i++) {
        Token current = tokens.arr[i];
        if(current.is_operator && current.operation == LEFT_PARENTHESIS) {
            stack_push(operator_stack, current);
        }
        else if(current.is_operator && current.operation == RIGHT_PARENTHESIS) {
            Token top_op;
            while(!stack_is_empty(operator_stack)) {
                stack_pop(operator_stack, &top_op);
                if(top_op.is_operator && top_op.operation == LEFT_PARENTHESIS) break;
                apply_operator(output_stack, top_op);
            }
        }
        else if(current.is_var) stack_push(output_stack, current);
        else if(current.is_digit) {
            Token new_token = { .is_digit = true, };
            mpfr_init2(new_token.digits, MIN_BITS);
            mpfr_set(new_token.digits, current.digits, MPFR_RNDN);
            stack_push(output_stack, new_token);
        } else {
            Token top_op;
            while(!stack_is_empty(operator_stack)
                && stack_peek(operator_stack, &top_op)
                && (top_op.is_operator)
                && ((top_op.precedence > current.precedence)
                || (top_op.precedence == current.precedence && !current.is_right_associative))) {
                stack_pop(operator_stack, &top_op);
                apply_operator(output_stack, top_op);
            }
            stack_push(operator_stack, current);
        }
    }
    // Process remaining operators
    Token op;
    while(stack_pop(operator_stack, &op)) {
        apply_operator(output_stack, op);
    }

    Token final_result;
    bool success = false;
    if(stack_pop(output_stack, &final_result) && stack_is_empty(output_stack)) {
        if(final_result.is_digit) {
            mpfr_set(result, final_result.digits, MPFR_RNDN);
            mpfr_clear(final_result.digits);
            success = true;
        } else if(final_result.is_var) {
            if(vars[final_result.var - 'A'].is_initialized) {
                mpfr_set(result, vars[final_result.var - 'A'].var, MPFR_RNDN);
                success = true;
            } else {
                fprintf(stderr, "Variable '%c' is not defined\n", final_result.var);
            }
        }
    }
    // Cleanup
    while(stack_pop(output_stack, &final_result)) {
        if (final_result.is_digit) {
            mpfr_clear(final_result.digits);
        }
    }
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
    read_vars();
    printf("Start typing an expression or enter 'h' for help\n");
    while(true) {
        printf(">  ");
        char *expression = str_input(stdin);
        if(!*(expression)) {
            printf("Empty expression\n");
            free(expression);
            continue;
        }
        if(!strcmp(expression, "h")) {
            printf("You can use parenthesis '()'\nYou can use A-Z as variables\nEnter 'q' to quit\n");
            free(expression);
            continue;
        }
        if(!strcmp(expression, "q")) {
            free(expression);
            write_all_vars();
            cleanup_vars();
            return EXIT_SUCCESS;
        } else {
            // remove the spaces
            size_t read = 0, write = 0;
            while(expression[read] != '\0') {
                if (expression[read++] != ' ') {
                    expression[write++] = expression[read - 1];
                }
            }
            expression[write] = '\0';
        }
        mpfr_t result;
        mpfr_init2(result, MIN_BITS);
        bool success = calculate_infix(result, expression);
        if(success) {
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
