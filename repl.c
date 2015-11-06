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
	char *sym;
	char *err;
} nval;

typedef struct lval {
	int type;
	nval val;
	int count;
	struct lval **cells;
} lval;

enum { LVAL_ERR, LVAL_NUM, LVAL_FNUM, LVAL_SYM, LVAL_SEXPR };

#define LVAL_NUMBER_VALUE(x) ((*x).type == LVAL_FNUM ? (*x).val.fnum : (*x).val.num)

lval* lval_read_num(mpc_ast_t*);
lval* lval_read(mpc_ast_t*);
lval* lval_add(lval*, lval*);
void lval_del(lval*);
void lval_expr_print(lval*, char, char);
void lval_print(lval*);
void lval_println(lval*);

lval* lval_err(char *s) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_ERR;
	v->val.err = malloc(strlen(s) + 1);
	strcpy(v->val.err, s);
	return v;
}

lval* lval_num(long x) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_NUM;
	v->val.num = x;
	return v;
}

lval* lval_fnum(double x) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_FNUM;
	v->val.fnum = x;
	return v;
}

lval* lval_sym(char *s) {
	// printf("allocationg symbol: %s\n", s);
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_SYM;
	v->val.sym = malloc(strlen(s) + 1);
	strcpy(v->val.sym, s);
	return v;
}

lval* lval_sexpr(void) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cells = NULL;
	return v;
}

void lval_del(lval *v) {
	switch(v->type) {
		case LVAL_NUM:
		case LVAL_FNUM:
			break;
		case LVAL_ERR:
			free(v->val.err);
			break;
		case LVAL_SYM:
			free(v->val.sym);
			break;
		case LVAL_SEXPR:
			for(int i = 0; i < v->count; i++) {
				lval_del(v->cells[i]);
			}
			free(v->cells);
			break;
	}
	free(v);
}

lval* lval_read_num(mpc_ast_t *t) {
	if (strstr(t->contents, ".")) {
		errno = 0;
		double x = strtod(t->contents, NULL);
		return errno != ERANGE ? lval_fnum(x) : lval_err("invalid number");
	}
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t *t) {
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

	lval *x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

lval* lval_add(lval *v, lval *x) {
	v->count++;
	v->cells = realloc(v->cells, (sizeof(lval*) * v->count));
	v->cells[v->count - 1] = x;
	return v;
}

void lval_expr_print(lval *v, char open, char close) {
	putchar(open);
	for(int i = 0; i < v->count; i++) {
		lval_print(v->cells[i]);

		if (i != (v->count - 1)) {
			putchar(' ');
		}
	}
	putchar(close);
}

void lval_print(lval *v) {
	switch(v->type) {
		case LVAL_NUM:
			printf("%li", v->val.num);
			return;
		case LVAL_FNUM:
			printf("%lf", v->val.fnum);
			return;
		case LVAL_ERR:
			printf("Error: %s", v->val.err);
			return;
		case LVAL_SYM:
			printf("%s", v->val.sym);
			return;
		case LVAL_SEXPR:
			lval_expr_print(v, '(', ')');
	}
}

void lval_println(lval *v) { lval_print(v); putchar('\n'); }

// lval eval_op(lval x, char *op, lval y) {
// 	if (x->type == LVAL_ERR) { return x; }
// 	if (y->type == LVAL_ERR) { return y; }

// 	if (x->type == LVAL_FNUM || y->type == LVAL_FNUM) {
// 		if (strcmp(op, "+") == 0) {
// 			return lval_fnum(LVAL_NUMBER_VALUE(x) + LVAL_NUMBER_VALUE(y));
// 		}
// 		if (strcmp(op, "-") == 0) {
// 			return lval_fnum(LVAL_NUMBER_VALUE(x) - LVAL_NUMBER_VALUE(y));
// 		}
// 		if (strcmp(op, "*") == 0) {
// 			return lval_fnum(LVAL_NUMBER_VALUE(x) * LVAL_NUMBER_VALUE(y));
// 		}
// 		if (strcmp(op, "%") == 0) {
// 			return lval_err(LERR_BAD_OP);
// 		}
// 		if (strcmp(op, "/") == 0) { 
// 			return LVAL_NUMBER_VALUE(y) == 0 ? lval_err(LERR_DIV_ZERO) : lval_fnum(LVAL_NUMBER_VALUE(x) / LVAL_NUMBER_VALUE(y));
// 		}
// 	}

// 	if (strcmp(op, "+") == 0) { return lval_num(x->val.num + y->val.num); }
// 	if (strcmp(op, "-") == 0) { return lval_num(x->val.num - y->val.num); }
// 	if (strcmp(op, "*") == 0) { return lval_num(x->val.num * y->val.num); }
// 	if (strcmp(op, "/") == 0) { 
// 		return y->val.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x->val.num / y->val.num);
// 	}
// 	if (strcmp(op, "%") == 0) { return lval_num(x->val.num % y->val.num); }
// 	return lval_err(LERR_BAD_OP);
// }

// lval eval(mpc_ast_t *t) {
// 	if (strstr(t->tag, "number")) {
// 		if (strstr(t->contents, ".")) {
// 			errno = 0;
// 			double x = strtod(t->contents, NULL);
// 			return errno != ERANGE ? lval_fnum(x) : lval_err(LERR_BAD_NUM);
// 		}
// 		errno = 0;
// 		long x = strtol(t->contents, NULL, 10);
// 		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
// 	}
	
// 	char *op = t->children[1]->contents;

// 	lval x = eval(t->children[2]);

// 	int i = 3;
// 	while(strstr(t->children[i]->tag, "expr")) {
// 		x = eval_op(x, op, eval(t->children[i]));
// 		i++;
// 	}
// 	return x;
// }



int main(int argc, char** argv) {
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Symbol = mpc_new("symbol");
	mpc_parser_t *Sexpr = mpc_new("sexpr");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lispy = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                          \
			number : /-?[0-9]+(\\.[0-9]+)?/ ;        \
			symbol : '+' | '-' | '*' | '/' | '%' ;   \
			sexpr  : '(' <expr>* ')' ;               \
			expr   : <number> | <symbol> | <sexpr> ; \
			lispy  : /^/ <expr>* /$/ ;               \
		",
		Number, Symbol, Sexpr, Expr, Lispy);

	puts("Lispy Version 0.0.0.0.1");
	puts("Press Ctrl+c to exit\n");

	while(1) {
		char *input = readline("lispy> ");
		add_history(input);

		mpc_result_t r;
		if(mpc_parse("<stdin>", input, Lispy, &r)) {
			// lval result = eval(r.output);
			// lval_println(result);

			lval *x = lval_read(r.output);
			lval_println(x);
			lval_del(x);

			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		// printf("No, you're a %s\n", input);
		free(input);
 }

 mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
 return 0;
}