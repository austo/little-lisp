CC = gcc
DEPS = deps
MPC_DIR = $(DEPS)/mpc
CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-parameter
OBJDIR := out

mpc:
	$(CC) -c $(CFLAGS) $(MPC_DIR)/mpc.c

repl: mpc $(OBJDIR)
	$(CC) $(CFLAGS) -I$(DEPS) repl.c mpc.o -ledit -lm -o $(OBJDIR)/repl

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -f $(OBJDIR)/*
	rm -f *.o

.PHONY:
	clean