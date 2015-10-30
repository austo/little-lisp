CC = gcc

prompt:
	$(CC) -std=c99 -Wall prompt.c -ledit -o out/prompt

clean:
	rm out/*

.PHONY:
	clean