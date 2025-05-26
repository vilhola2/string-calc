#include "structs.h"
#include <stdlib.h>

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

