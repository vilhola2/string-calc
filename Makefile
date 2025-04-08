NAME=calc

CC=clang
CFLAGS=-g -Wall -Wextra -Werror -pedantic -std=c23
LIBFLAGS=-lmpfr -lgmp -lm

BIN_DIR=

.PHONY: $(NAME) run
$(NAME): $(OBJ)
	$(CC) -o $(BIN_DIR)$(NAME) *.c $(LIBFLAGS) $(CFLAGS)

run: $(NAME)
	./$(NAME)
