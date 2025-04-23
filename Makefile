NAME=calc

CC=clang
CFLAGS=-g -Wall -Wextra -Werror -pedantic -std=c23
LIBFLAGS=-lmpfr -lgmp -lm

BIN_DIR=
LIB_DIR=lib/

.PHONY: $(NAME) run lib
$(NAME):
	$(CC) -o $(BIN_DIR)$(NAME) *.c $(LIBFLAGS) $(CFLAGS)

run: $(NAME)
	./$(NAME)

lib:
	for f in *.c; do if [ "$$f" != "main.c" ]; then $(CC) -c $$f -fPIC $(CFLAGS); fi; done
	mv *.o $(LIB_DIR)
	ar -rcs $(LIB_DIR)lib$(NAME).a $(LIB_DIR)*.o
	cp $(NAME).h $(LIB_DIR)
	rm $(LIB_DIR)*.o
