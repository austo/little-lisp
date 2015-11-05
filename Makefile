CC = gcc
DEPS = deps
MPC_DIR = $(DEPS)/mpc

mpc:
	$(CC) -c -std=c99 -Wall $(MPC_DIR)/mpc.c

repl: mpc
	$(CC) -std=c99 -I$(DEPS) -Wall repl.c mpc.o -ledit -lm -o out/repl

clean:
	rm -f out/*
	rm -f *.o

.PHONY:
	clean