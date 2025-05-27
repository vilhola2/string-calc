#include <assert.h>
#include <gmp.h>
#include <mpfr.h>
#include <stdint.h>
#include <stdio.h>
#include "calc.h"
#include "defs.h"
#include "file_ops.h"
#include "structs.h"
#include "stack.h"
#include "lexer.h"

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
            if (!stack_push(output_stack, result)) mpfr_clear(result.digits);
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
    if (operand1.is_digit) mpfr_clear(operand1.digits);
    if (operand2.is_digit) mpfr_clear(operand2.digits);
}

CalculatorResult calculate_infix(const char *expression) {
    TokenArray tokens = tokenize(expression);
    if (!tokens.arr) return (CalculatorResult){0};
#ifdef DEBUG
    print_token_arr(&tokens);
#endif
    Stack *operator_stack = create_stack(tokens.len);
    Stack *output_stack = create_stack(tokens.len);
    if (!operator_stack || !output_stack) {
        free_token_array(&tokens);
        if (operator_stack) destroy_stack(operator_stack);
        if (output_stack) destroy_stack(output_stack);
        return (CalculatorResult){0};
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
    CalculatorResult calc_result = {.type = CALC_MPFR_STRING};
    if (stack_pop(output_stack, &final_result) && stack_is_empty(output_stack)) {
        if (result_is_boolean) {
            if (mpfr_cmp_ui(final_result.digits, 1) == 0) {
                calc_result.type = CALC_BOOLEAN_STRING;
                calc_result.str = "true";
            } else {
                calc_result.type = CALC_BOOLEAN_STRING;
                calc_result.str = "false";
            }
            mpfr_clear(final_result.digits);
        } else if (final_result.is_digit) {
            mpfr_asprintf(&calc_result.str, "%Rg", final_result.digits);
            mpfr_clear(final_result.digits);
        } else if (final_result.is_var) {
            if (vars[final_result.var - 'A'].is_initialized) {
                mpfr_asprintf(&calc_result.str, "%Rg", vars[final_result.var - 'A'].var);
            } else {
                fprintf(stderr, "calculate_infix: Variable '%c' is not defined\n", final_result.var);
                calc_result.type = CALC_ERROR;
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
    return calc_result;
}

