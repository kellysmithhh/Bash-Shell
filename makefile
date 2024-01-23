NAME = shell
FLAGS = -Wall -pedantic-errors

cl: $(NAME).c
	gcc -o $(NAME) $(FLAGS) $(NAME).c

run: cl
	ulimit -u 30; ./$(NAME)

clean:
	rm -f $(NAME)
