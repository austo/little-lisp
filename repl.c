#include <stdio.h>
#include <stdlib.h>

#include "mpc/mpc.h"

#ifdef _WIN32
#include <string.h>

enum { BUFFER_SIZE 2048 };

static char buffer[BUFFER_SIZE];

char *readline(char *prompt) {
	fputs(prompt, stdout);
	fgets(buffer, BUFFER_SIZE, stdin);
	char *cpy = malloc(strlen(buffer) + 1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy) - 1] = '\0';
	return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#ifdef __linux__
#include <editline/history.h>
#endif
#endif

typedef union {
	long num;
	double fnum;
	int err;
} nval;

typedef struct {
	int type;
	nval val;
} lval;

enum { LVAL_NUM, LVAL_FNUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

#define GET_LVAL_VALUE(x) ((x).type == LVAL_FNUM ? (x).val.fnum : (x).val.num)

lval lval_num(long x) {
	lval v;
	v.type = LVAL_NUM;
	v.val.num = x;
	return v;
}

lval lval_fnum(double x) {
	lval v;
	v.type = LVAL_FNUM;
	v.val.fnum = x;
	return v;
}

lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.val.err = x;
	return v;
}

void lval_print(lval v) {
	switch(v.type) {
		case LVAL_NUM:
			printf("%li", v.val.num);
			return;
		case LVAL_FNUM:
			printf("%lf\n", v.val.fnum);
			return;
		case LVAL_ERR:
			switch(v.val.err) {
				case LERR_DIV_ZERO:
					printf("Error: Divison by zero");
					return;
				case LERR_BAD_OP:
					printf("Error: invalid operator");
					return;
				case LERR_BAD_NUM:
					printf("Error: bad number");
					return;
			}
	}
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_op(lval x, char *op, lval y) {
	if (x.type == LVAL_ERR) { return x; }
	if (y.type == LVAL_ERR) { return y; }

	if (x.type == LVAL_FNUM || y.type == LVAL_FNUM) {
		if (strcmp(op, "+") == 0) {
			return lval_fnum(GET_LVAL_VALUE(x) + GET_LVAL_VALUE(y));
		}
		if (strcmp(op, "-") == 0) {
			return lval_fnum(GET_LVAL_VALUE(x) - GET_LVAL_VALUE(y));
		}
		if (strcmp(op, "*") == 0) {
			return lval_fnum(GET_LVAL_VALUE(x) * GET_LVAL_VALUE(y));
		}
		if (strcmp(op, "%") == 0) {
			return lval_err(LERR_BAD_OP);
		}
		if (strcmp(op, "/") == 0) { 
			return GET_LVAL_VALUE(y) == 0 ? lval_err(LERR_DIV_ZERO) : lval_fnum(GET_LVAL_VALUE(x) / GET_LVAL_VALUE(y));
		}
	}

	if (strcmp(op, "+") == 0) { return lval_num(x.val.num + y.val.num); }
	if (strcmp(op, "-") == 0) { return lval_num(x.val.num - y.val.num); }
	if (strcmp(op, "*") == 0) { return lval_num(x.val.num * y.val.num); }
	if (strcmp(op, "/") == 0) { 
		return y.val.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.val.num / y.val.num);
	}
	if (strcmp(op, "%") == 0) { return lval_num(x.val.num % y.val.num); }
	return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {
	if (strstr(t->tag, "number")) {
		if (strstr(t->contents, ".")) {
			errno = 0;
			double x = strtod(t->contents, NULL);
			return errno != ERANGE ? lval_fnum(x) : lval_err(LERR_BAD_NUM);
		}
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
	}
	
	char *op = t->children[1]->contents;

	lval x = eval(t->children[2]);

	int i = 3;
	while(strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}
	return x;
}



int main(int argc, char** argv) {
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Operator = mpc_new("operator");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lispy = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                                \
			number   : /-?[0-9]+(\\.[0-9]+)?/ ;                            \
			operator : '+' | '-' | '*' | '/' | '%' ;                       \
			expr     : <number> | '(' <operator> <expr>+ ')' ;             \
			lispy    : /^/ <operator> <expr>+ /$/ ;                        \
		",
		Number, Operator, Expr, Lispy);

	puts("Lispy Version 0.0.0.0.1");
	puts("Press Ctrl+c to exit\n");

	while(1) {
		char *input = readline("lispy> ");
		add_history(input);

		mpc_result_t r;
		if(mpc_parse("<stdin>", input, Lispy, &r)) {
			lval result = eval(r.output);
			lval_println(result);
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		// printf("No, you're a %s\n", input);
		free(input);
 }

 mpc_cleanup(4, Number, Operator, Expr, Lispy);
 return 0;
}