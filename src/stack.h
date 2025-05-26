#ifndef STACK_H
#define STACK_H

#include "structs.h"

Stack *create_stack(size_t capacity);
void destroy_stack(Stack *stack);
bool stack_push(Stack *stack, Token item);
bool stack_pop(Stack *stack, Token *item);
bool stack_peek(const Stack *stack, Token *item);

[[maybe_unused]] static inline bool stack_is_empty(const Stack *stack) {
    return stack->top == 0;
}

#endif

