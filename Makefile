CC = gcc
DEPS = deps
MPC_DIR = $(DEPS)/mpc
CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-parameter
OBJDIR := out

mpc:
	$(CC) -c $(CFLAGS) $(MPC_DIR)/mpc.c

lispy: mpc $(OBJDIR)
	$(CC) $(CFLAGS) -I$(DEPS) lispy.c mpc.o -ledit -lm -o $(OBJDIR)/lispy

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Miscellaneous .c files
%: %.c $(OBJDIR)
	$(CC) $(CFLAGS) $< -o $(OBJDIR)/$@

clean:
	rm -f $(OBJDIR)/*
	rm -f *.o

.PHONY:
	clean