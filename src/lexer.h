#ifndef LEXER_H
#define LEXER_H

#include "structs.h"

void free_token_array(TokenArray *arr);
void print_token_arr(TokenArray *token_arr);
TokenArray tokenize(const char *str);

#endif
