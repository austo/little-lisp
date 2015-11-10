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

enum { LVAL_ERR, LVAL_NUM, LVAL_FNUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

#define LVAL_NUMBER_VALUE(x) ((*x).type == LVAL_FNUM ? (*x).val.fnum : (*x).val.num)
#define LASSERT(args, cond, err) \
	if (!(cond)) { lval_del(args); return lval_err(err); }

// TODO: move to header file

lval *lval_read_num(mpc_ast_t *);
lval *lval_read(mpc_ast_t *);
lval *lval_add(lval*, lval *);
lval *lval_eval_sexpr(lval *);
lval *lval_eval(lval *);
lval *lval_pop(lval *, int);
lval *lval_take(lval *, int);
lval *lval_join(lval *, lval *);
lval *builtin(lval *, char *);
lval *builtin_head(lval *);
lval *builtin_tail(lval *);
lval *builtin_list(lval *);
lval *builtin_eval(lval *);
lval *builtin_join(lval *);
lval *builtin_op(lval *, char*);

void lval_del(lval *);
void lval_expr_print(lval *, char, char);
void lval_print(lval *);
void lval_println(lval *);


lval *
lval_err(char *s) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_ERR;
	v->val.err = malloc(strlen(s) + 1);
	strcpy(v->val.err, s);
	v->count = 0;
	v->cells = NULL;
	return v;
}

lval *
lval_num(long x) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_NUM;
	v->val.num = x;
	v->count = 0;
	v->cells = NULL;
	return v;
}

lval *
lval_fnum(double x) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_FNUM;
	v->val.fnum = x;
	v->count = 0;
	v->cells = NULL;
	return v;
}

lval *
lval_sym(char *s) {
	// printf("allocationg symbol: %s\n", s);
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_SYM;
	v->val.sym = malloc(strlen(s) + 1);
	strcpy(v->val.sym, s);
	v->count = 0;
	v->cells = NULL;
	return v;
}

lval *
lval_sexpr(void) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cells = NULL;
	return v;
}

lval *
lval_qexpr(void) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cells = NULL;
	return v;
}

void
lval_del(lval *v) {
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
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			for(int i = 0; i < v->count; i++) {
				lval_del(v->cells[i]);
			}
			free(v->cells);
			break;
	}
	free(v);
}

lval *
lval_read_num(mpc_ast_t *t) {
	if (strstr(t->contents, ".")) {
		errno = 0;
		double x = strtod(t->contents, NULL);
		return errno != ERANGE ? lval_fnum(x) : lval_err("invalid number");
	}
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *
lval_read(mpc_ast_t *t) {
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

	lval *x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
	if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

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

lval *
lval_add(lval *v, lval *x) {
	v->count++;
	v->cells = realloc(v->cells, (sizeof(lval*) * v->count));
	v->cells[v->count - 1] = x;
	return v;
}

void
lval_expr_print(lval *v, char open, char close) {
	putchar(open);
	for(int i = 0; i < v->count; i++) {
		lval_print(v->cells[i]);

		if (i != (v->count - 1)) {
			putchar(' ');
		}
	}
	putchar(close);
}

void
lval_print(lval *v) {
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
			return;
		case LVAL_QEXPR:
			lval_expr_print(v, '{', '}');
	}
}

void
lval_println(lval *v) { lval_print(v); putchar('\n'); }

lval *
lval_eval_sexpr(lval *v) {
	for (int i = 0; i < v->count; i++) {
		v->cells[i] = lval_eval(v->cells[i]);
	}

	// check errors
	for(int i = 0; i < v->count; i++) {
		if (v->cells[i]->type == LVAL_ERR) { return lval_take(v, 0); }
	}

	// empty expression
	if (v->count == 0) { return v; }

	// single expression
	if (v->count == 1) { return lval_take(v, 0); }

	// ensure first element is symbol
	lval *f = lval_pop(v, 0);
	if (f->type != LVAL_SYM) {
		lval_del(f);
		lval_del(v);
		return lval_err("S-expression does not start with symbol");
	}

	// call builtin with operator
	lval *result = builtin(v, f->val.sym);
	lval_del(f);
	return result;
}

lval *
lval_eval(lval *v) {
	// evaluate S-expressions
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
	// other types remain the same
	return v;
}

lval *
lval_pop(lval *v, int i) {
	lval *x = v->cells[i];

	// shift memory after item at i
	memmove(&v->cells[i], &v->cells[i + 1], sizeof(lval*) * (v->count-i-1));

	v->count--;

	// reallocate memory
	v->cells = realloc(v->cells, sizeof(lval*) * v->count);
	return x;
}

lval *
lval_take(lval *v, int i) {
	lval *x = lval_pop(v, i);
	lval_del(v);
	return x;
}

lval *
builtin(lval *a, char *func) {
	if (strcmp("list", func) == 0) { return builtin_list(a); }
	if (strcmp("head", func) == 0) { return builtin_head(a); }
	if (strcmp("tail", func) == 0) { return builtin_tail(a); }
	if (strcmp("join", func) == 0) { return builtin_join(a); }
	if (strcmp("eval", func) == 0) { return builtin_eval(a); }

	if (strstr("+-*/%", func)) { return builtin_op(a, func); }
	lval_del(a);
	return lval_err("unknown function");
}

lval *
builtin_head(lval *a) {
	LASSERT(a, a->count == 1,
		"function 'head' passed too many arguments");

	LASSERT(a, a->cells[0]->type == LVAL_QEXPR,
		"function 'head' passed incorrect argument type");

	LASSERT(a, a->cells[0]->count != 0,
		"function 'head' passed '{}'");

	// take first argument
	lval *v = lval_take(a, 0);

	// delete all elements that aren't head and return;
	while(v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval *
builtin_tail(lval *a) {
	LASSERT(a, a->count == 1,
		"function 'tail' passed too many arguments");

	LASSERT(a, a->cells[0]->type == LVAL_QEXPR,
		"function 'tail' passed incorrect argument type");

	LASSERT(a, a->cells[0]->count != 0,
		"function 'tail' passed '{}'");

	// take first argument
	lval *v = lval_take(a, 0);

	// delete first element and return
	lval_del((lval_pop(v, 0)));
	return v;
}

lval *
builtin_list(lval *a) {
	a->type = LVAL_QEXPR;
	return a;
}

lval *
builtin_eval(lval *a) {
	LASSERT(a, a->count == 1,
		"function 'eval' passed too many arguments");

	LASSERT(a, a->cells[0]->type == LVAL_QEXPR,
		"function 'eval' passed incorrect argument type");

	lval *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(x);
}

lval *
builtin_join(lval *a) {
	for (int i = 0; i < a->count; i++) {
		LASSERT(a, a->cells[i]->type == LVAL_QEXPR,
			"function 'join' passed incorrect argument type");
	}

	lval *x = lval_pop(a, 0);

	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}

	lval_del(a);
	return x;
}

lval *
lval_join(lval *x, lval *y) {
	// for each cell in 'y', add it to 'x'
	while (y->count) {
		x = lval_add(x, lval_pop(y, 0));
	}

	// delete empty 'y' and return 'x'
	lval_del(y);
	return x;
}

lval *
builtin_op(lval *a, char *op) {
	// ensure all operands are numbers
	for(int i = 0; i < a->count; i++) {
		if (a->cells[i]->type != LVAL_NUM && a->cells[i]->type != LVAL_FNUM) {
			lval_del(a);
			return lval_err("cannot operate on non-number");
		}
	}

	lval *x = lval_pop(a, 0);

	// if no arguments and '-', perform unary negation
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		if (x->type == LVAL_NUM) {
			x->val.num = -x->val.num;
		}
		if (x->type == LVAL_FNUM) {
			x->val.fnum = -x->val.fnum;
		}
	}

	while(a->count > 0) {
		lval *y = lval_pop(a, 0);

		if (x->type == LVAL_FNUM || y->type == LVAL_FNUM) {
			if (x->type == LVAL_NUM) {
				lval *tmp = lval_fnum((double) x->val.num);
				tmp->cells = x->cells;
				tmp->count = x->count;
				lval_del(x);
				x = tmp;
			}
			if (strcmp(op, "+") == 0) {
				x->val.fnum = (x->val.fnum + LVAL_NUMBER_VALUE(y));
			}
			if (strcmp(op, "-") == 0) {
				x->val.fnum = (x->val.fnum - LVAL_NUMBER_VALUE(y));
			}
			if (strcmp(op, "*") == 0) {
				x->val.fnum = (x->val.fnum * LVAL_NUMBER_VALUE(y));
			}
			if (strcmp(op, "/") == 0) {
				if (((int)(LVAL_NUMBER_VALUE(y))) == 0) {
					lval_del(x);
					lval_del(y);
					x = lval_err("division by zero");
					break;
				}
				x->val.fnum /= LVAL_NUMBER_VALUE(y);
			}
			if (strcmp(op, "%") == 0) {
				lval_del(x);
				lval_del(y);
				x = lval_err("bad floating point operation");
				break;
			}
		} else {
			if (strcmp(op, "+") == 0) { x->val.num += y->val.num; }
			if (strcmp(op, "-") == 0) { x->val.num -= y->val.num; }
			if (strcmp(op, "*") == 0) { x->val.num *= y->val.num; }
			if (strcmp(op, "/") == 0) { 
				if (y->val.num == 0) {
					lval_del(x);
					lval_del(y);
					x = lval_err("division by zero");
					break;
				}
				x->val.num /= y->val.num;
			}
			if (strcmp(op, "%") == 0) { x->val.num %= y->val.num; }
		}
		
		lval_del(y);
	}

	lval_del(a);
	return x;
}


int
main(int argc, char** argv) {
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Symbol = mpc_new("symbol");
	mpc_parser_t *Sexpr = mpc_new("sexpr");
	mpc_parser_t *Qexpr = mpc_new("qexpr");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lispy = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                    \
			number : /-?[0-9]+(\\.[0-9]+)?/ ;                  \
			symbol : /[a-zA-Z0-9_+\\-*\\/%\\\\=<>!&]+/ ;       \
			sexpr  : '(' <expr>* ')' ;                         \
			qexpr  : '{' <expr>* '}' ;                         \
			expr   : <number> | <symbol> | <sexpr> | <qexpr>;  \
			lispy  : /^/ <expr>* /$/ ;                         \
		",
		Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	puts("Lispy Version 0.0.0.0.1");
	puts("Press Ctrl+c to exit\n");

	while(1) {
		char *input = readline("lispy> ");
		add_history(input);

		mpc_result_t r;
		if(mpc_parse("<stdin>", input, Lispy, &r)) {

			lval *x = lval_eval(lval_read(r.output));
			lval_println(x);
			lval_del(x);

			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
 }

 mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
 return 0;
}