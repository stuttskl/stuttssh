all: smallsh.c
	gcc --std=gnu99 -o smallsh smallsh.c

clean:
	$(RM) smallsh
