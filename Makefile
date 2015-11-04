CC = gcc

repl:
	$(CC) -std=c99 -Wall repl.c -ledit -o out/repl

clean:
	rm out/*

.PHONY:
	clean