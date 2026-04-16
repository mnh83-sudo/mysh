CC     = gcc
CFLAGS = -Wall -Wextra -g
 
all: mysh
 
mysh: mysh.c
	$(CC) $(CFLAGS) -o mysh mysh.c
 
clean:
	rm -f mysh