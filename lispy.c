#include <stdio.h>
#include <stdlib.h>

#include "mpc/mpc.h"
#include "lispy.h"

mpc_parser_t *Number;
mpc_parser_t *Symbol;
mpc_parser_t *String;
mpc_parser_t *Comment;
mpc_parser_t *Sexpr;
mpc_parser_t *Qexpr;
mpc_parser_t *Expr;
mpc_parser_t *Lispy;

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

enum { LVAL_ERR, LVAL_NUM, LVAL_FNUM, LVAL_SYM, LVAL_STR,
	LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR, LVAL_LAMBDA };


struct lcontext {
	lenv *env;
	lval *formals;
	lval *body;
};

typedef union {
	long num;
	double fnum;
	char *sym;
	char *str;
	char *err;
	lbuiltin builtin;
	lcontext *context;
} nval;

struct lval {
	int type;
	nval val;
	int count;
	struct lval **cells;
};

struct lenv {
	lenv *par;
	int count;
	char **syms;
	lval **vals;
};

#define LVAL_NUMBER_VALUE(x) ((*x).type == LVAL_FNUM ? (*x).val.fnum : (*x).val.num)

#define LASSERT(args, cond, fmt, ...) \
	if (!(cond)) { lval *err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
	LASSERT(args, args->cells[index]->type == expect, \
		"function %s passed incorrect type for argument %i. Got %s, expected %s.", \
		func, index, ltype_name(args->cells[index]->type), ltype_name(expect))

#define LASSERT_NUM_TYPE(func, args, index) \
	LASSERT(args, ((args->cells[index]->type == LVAL_NUM) || (args->cells[index]->type == LVAL_FNUM)), \
		"function '%s' passed incorrect type for argument %i. Got %s, expected Number.", \
		func, index, ltype_name(args->cells[index]->type))

#define LASSERT_NUM(func, args, num) \
	LASSERT(args, args->count == num, \
		"function '%s' passed incorrect number of arguments. Got %i, expected %i." , \
		func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
	LASSERT(args, args->cells[index]->count != 0, \
		"function %s passed {} for argument %i.", func, index);

lenv *
lenv_new(void) {
	lenv *e = malloc(sizeof(*e));
	e->par = NULL;
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void
lenv_del(lenv *e) {
	for (int i = 0; i < e->count; i++) {
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lval *
lval_err(char *fmt, ...) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_ERR;

	va_list va;
	va_start(va, fmt);

	v->val.err = malloc(512);

	vsnprintf(v->val.err, 511, fmt, va);

	v->val.err = realloc(v->val.err, strlen(v->val.err) + 1);

	va_end(va);

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

lval *lval_str(char *s) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_STR;
	v->val.str = malloc(strlen(s) + 1);
	strcpy(v->val.str, s);
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

lval *
lval_fun(lbuiltin fun) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_FUN;
	v->val.builtin = fun;
	return v;
}

lval *
lval_lambda(lval *formals, lval *body) {
	lval *v = malloc(sizeof(*v));
	v->type = LVAL_LAMBDA;

	lcontext *c = malloc(sizeof (*c));
	c->env = lenv_new();
	c->formals = formals;
	c->body = body;

	v->val.context = c;
	return v;
}

lval *
lenv_get(lenv *e, lval *k) {
	// iterate over all items in the environment
	for (int i = 0; i < e->count; i++) {
		if (strcmp(e->syms[i], k->val.sym) == 0){
			return lval_copy(e->vals[i]);
		}
	}
	// if no symbol, check in parent, otherwise error
	if (e->par) {
		return lenv_get(e->par, k);
	}
	return lval_err("Unbound symbol '%s',", k->val.sym);
}

void
lenv_put(lenv *e, lval *k, lval *v) {
	// iterate over all item in env. to see if variable exists
	for (int i = 0; i < e->count; i++) {
		// if variable is found, delete & replace w/user defined var
		if (strcmp(e->syms[i], k->val.sym) == 0) {
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}

	// if no entry found, add new entry
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval *) * e->count);
	e->syms = realloc(e->syms, sizeof(char *) * e->count);

	e->vals[e->count - 1] = lval_copy(v);
	e->syms[e->count - 1] = malloc(strlen(k->val.sym) + 1);
	strcpy(e->syms[e->count - 1], k->val.sym);
}

void
lval_del(lval *v) {
	switch(v->type) {
		case LVAL_NUM:
		case LVAL_FNUM:
		case LVAL_FUN:
			break;
		case LVAL_LAMBDA:
			lenv_del(v->val.context->env);
			lval_del(v->val.context->formals);
			lval_del(v->val.context->body);
			free(v->val.context);
			break;
		case LVAL_ERR:
			free(v->val.err);
			break;
		case LVAL_SYM:
			free(v->val.sym);
			break;
		case LVAL_STR:
			free(v->val.str);
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
lval_read_str(mpc_ast_t *t) {
	// cut off final quote character
	t->contents[strlen(t->contents) - 1] = '\0';
	// copy the string while removing first quote character
	char *unescaped = malloc(strlen(t->contents + 1) + 1);
	strcpy(unescaped, t->contents + 1);
	// pass through unescape function
	unescaped = mpcf_unescape(unescaped);
	// construct new lval using the string
	lval *str = lval_str(unescaped);
	// free string and return
	free(unescaped);
	return str;
}

lval *
lval_read(mpc_ast_t *t) {
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	if (strstr(t->tag, "string")) { return lval_read_str(t); }

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
		if (strstr(t->children[i]->tag, "comment")) { continue; }
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
		case LVAL_FUN:
			printf("<function>");
			return;
		case LVAL_LAMBDA:
			printf("(\\ ");
			lval_print(v->val.context->formals);
			putchar(' ');
			lval_print(v->val.context->body);
			break;
		case LVAL_ERR:
			printf("Error: %s", v->val.err);
			return;
		case LVAL_SYM:
			printf("%s", v->val.sym);
			return;
		case LVAL_STR:
			lval_print_str(v);
			return;
		case LVAL_SEXPR:
			lval_expr_print(v, '(', ')');
			return;
		case LVAL_QEXPR:
			lval_expr_print(v, '{', '}');
	}
}

lval *
lval_copy(lval *v) {
	lval *x = malloc(sizeof(*x));
	x->type = v->type;
	x->count = 0;
	x->cells = NULL;

	switch(v->type) {
		case LVAL_NUM:
			x->val.num = v->val.num;
			break;
		case LVAL_FNUM:
			x->val.fnum = v->val.fnum;
			break;
		case LVAL_FUN:
			x->val.builtin = v->val.builtin;
			break;
		case LVAL_LAMBDA:
			x->val.context = malloc(sizeof(*(x->val.context)));
			x->val.context->env = lenv_copy(v->val.context->env);
			x->val.context->formals = lval_copy(v->val.context->formals);
			x->val.context->body = lval_copy(v->val.context->body);
			break;
		case LVAL_SYM:
			x->val.sym = malloc(strlen(v->val.sym) + 1);
			strcpy(x->val.sym, v->val.sym);
			break;
		case LVAL_STR:
			x->val.str = malloc(strlen(v->val.str) + 1);
			strcpy(x->val.str, v->val.str);
		case LVAL_ERR:
			x->val.err = malloc(strlen(v->val.err) + 1);
			strcpy(x->val.err, v->val.err);
			break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			x->count = v->count;
			x->cells = malloc(sizeof(lval *) * x->count);
			for (int i = 0; i < x->count; i++) {
				x->cells[i] = lval_copy(v->cells[i]);
			}
			break;
	}
	return x;
}

void
lval_println(lval *v) { lval_print(v); putchar('\n'); }

void
lval_print_str(lval *v) {
	// make a copy of the string
	char *escaped = malloc(strlen(v->val.str) + 1);
	strcpy(escaped, v->val.str);
	// pass through escape function
	escaped = mpcf_escape(escaped);
	// print between quotes
	printf("\"%s\"", escaped);
	// free copied string
	free(escaped);
}

char *
ltype_name(int t) {
	switch (t) {
		case LVAL_FUN:
			return "Function";
		case LVAL_NUM:
		case LVAL_FNUM:
			return "Number";
		case LVAL_ERR:
			return "Error";
		case LVAL_SYM:
			return "Symbol";
		case LVAL_STR:
			return "String";
		case LVAL_SEXPR:
			return "S-Expression";
		case LVAL_QEXPR:
			return "Q-Expression";
		default:
			return "Unknown";
	}
}

lval *
lval_eval(lenv *e, lval *v) {
	if (v->type == LVAL_SYM) {
		lval *x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	// evaluate S-expressions
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
	// other types remain the same
	return v;
}

lval *
lval_eval_sexpr(lenv *e, lval *v) {
	for (int i = 0; i < v->count; i++) {
		v->cells[i] = lval_eval(e, v->cells[i]);
	}

	// check errors
	for(int i = 0; i < v->count; i++) {
		if (v->cells[i]->type == LVAL_ERR) { return lval_take(v, i); }
	}

	// empty expression
	if (v->count == 0) { return v; }

	// single expression
	if (v->count == 1) { return lval_take(v, 0); }

	// ensure first element is function after evaluation
	lval *f = lval_pop(v, 0);
	if (f->type != LVAL_FUN && f->type != LVAL_LAMBDA) {
		lval *err = lval_err(
			"S-expression starts with incorrect type. "
			"Got %s, expected %s or %s.",
			ltype_name(f->type),
			ltype_name(LVAL_FUN),
			ltype_name(LVAL_LAMBDA));
		lval_del(f);
		lval_del(v);
		return err;
	}

	lval *result = lval_call(e, f, v);
	lval_del(f);
	return result;
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
lval_call(lenv *e, lval *f, lval *a) {
	// if builtin, call that
	if (f->type == LVAL_FUN) {
		return f->val.builtin(e, a);
	}

	int given = a->count;
	int total = f->val.context->formals->count;

	while(a->count) {
		// if we've run out of formal arguments to bind
		if (f->val.context->formals->count == 0) {
			lval_del(a); return lval_err("function passed too many arguments. "
				"Got %i, expeceted %i.", given, total);
		}

		// pop first symbol from the formals
		lval *sym = lval_pop(f->val.context->formals, 0);

		// deal with varargs symbol '&'
		if (strcmp(sym->val.sym, "&") == 0) {
			// ensure '&' is followed by another symbol
			if (f->val.context->formals->count != 1) {
				lval_del(a);
				return lval_err("function format invalid. "
					"Symbol '&' not followed by single symbol.");
			}

			// next formal should be bound to remaining arguments
			lval *nsym = lval_pop(f->val.context->formals, 0);
			lenv_put(f->val.context->env, nsym, builtin_list(e, a));
			lval_del(sym);
			lval_del(nsym);
			break;
		}

		// pop next argument from the list
		lval *val = lval_pop(a, 0);

		// bind a copy into the function's environment
		lenv_put(f->val.context->env, sym, val);

		// delete the symbol and value
		lval_del(sym);
		lval_del(val);
	}

	// argument list is now bound so can be cleaned up
	lval_del(a);

	// if '&' remains in formal list, bind to empty list
	if (f->val.context->formals->count > 0 &&
		strcmp(f->val.context->formals->cells[0]->val.sym, "&") == 0) {

		// check to make sure '&' is not passed invalidly
		if (f->val.context->formals->count != 2) {
			return lval_err("function format invalid. "
				"Symbol '&' not followed by single symbol.");
		}

		// pop and delete '&' symbol
		lval_del(lval_pop(f->val.context->formals, 0));

		// pop next symbol and create empty list
		lval *sym = lval_pop(f->val.context->formals, 0);
		lval *val = lval_qexpr();

		// bind to environment and delete
		lenv_put(f->val.context->env, sym, val);
		lval_del(sym);
		lval_del(val);
	}
 
	// if all formals have been bound, evaluate
	if (f->val.context->formals->count == 0) {
		f->val.context->env->par = e;

		// evaluate and return
		return builtin_eval(f->val.context->env,
			lval_add(lval_sexpr(), lval_copy(f->val.context->body)));
	} else {
		// return partially evaluated function
		return lval_copy(f);
	}


	// assign each argument fo each formal in order
	for (int i = 0; i < a->count; i++) {
		lenv_put(f->val.context->env,
			f->val.context->formals->cells[i], a->cells[i]);
	}

	lval_del(a);

	// set parent environment
	f->val.context->env->par = e;

	// evaluate the body
	return builtin_eval(f->val.context->env,
		lval_add(lval_sexpr(), lval_copy(f->val.context->body)));
}

lenv *
lenv_copy(lenv *e) {
	lenv *n = malloc(sizeof(*n));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char *) * n->count);
	n->vals = malloc(sizeof(lval *) * n->count);

	for(int i = 0; i < n->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = lval_copy(e->vals[i]);
	}
	return n;
}

void
lenv_def(lenv *e, lval *k, lval *v) {
	// iterate until e has no parent
	while (e->par) { e = e->par; }

	// put value in e
	lenv_put(e, k, v);
}


/* Builtins */

void
lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
	lval *k = lval_sym(name);
	lval *v = lval_fun(func);
	lenv_put(e, k, v);
	lval_del(k);
	lval_del(v);
}

void
lenv_add_builtins(lenv *e) {
	// variable functions
	lenv_add_builtin(e, "\\", builtin_lambda);
	lenv_add_builtin(e, "def", builtin_def);
	lenv_add_builtin(e, "=", builtin_put);

	// list fuctions
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);

	// mathematical functions
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
	lenv_add_builtin(e, "%", builtin_mod);

	lenv_add_builtin(e, "if", builtin_if);
	lenv_add_builtin(e, "==", builtin_eq);
	lenv_add_builtin(e, "!=", builtin_ne);
	lenv_add_builtin(e, ">", builtin_gt);
	lenv_add_builtin(e, "<", builtin_lt);
	lenv_add_builtin(e, ">=", builtin_ge);
	lenv_add_builtin(e, "<=", builtin_le);

	lenv_add_builtin(e, "load", builtin_load);
	lenv_add_builtin(e, "print", builtin_print);
	lenv_add_builtin(e, "error", builtin_error);
}

lval *
builtin_add(lenv *e, lval *a) {
	return builtin_op(e, a, "+");
}

lval *
builtin_sub(lenv *e, lval *a) {
	return builtin_op(e, a, "-");
}

lval *
builtin_mul(lenv *e, lval *a) {
	return builtin_op(e, a, "*");
}

lval *
builtin_div(lenv *e, lval *a) {
	return builtin_op(e, a, "/");
}

lval *
builtin_mod(lenv *e, lval *a) {
	return builtin_op(e, a, "%");
}

lval *
builtin_gt(lenv *e, lval *a) {
	return builtin_ord(e, a, ">");
}

lval *
builtin_lt(lenv *e, lval *a) {
	return builtin_ord(e, a, "<");
}

lval *
builtin_ge(lenv *e, lval *a) {
	return builtin_ord(e, a, ">=");
}

lval *
builtin_le(lenv *e, lval *a) {
	return builtin_ord(e, a, "<=");
}

lval *
builtin_put(lenv *e, lval *a) {
	return builtin_var(e, a, "=");
}

lval *builtin_def(lenv *e, lval *a) {
	return builtin_var(e, a, "def");
}

lval *
builtin_head(lenv *e, lval *a) {
	LASSERT_NUM("head", a, 1);
	LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
	LASSERT_NOT_EMPTY("head", a, 0);

	// take first argument
	lval *v = lval_take(a, 0);

	// delete all elements that aren't head and return;
	while(v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval *
builtin_tail(lenv *e, lval *a) {
	LASSERT_NUM("tail", a, 1);
	LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
	LASSERT_NOT_EMPTY("tail", a, 0);

	// take first argument
	lval *v = lval_take(a, 0);

	// delete first element and return
	lval_del((lval_pop(v, 0)));
	return v;
}

lval *
builtin_lambda(lenv *e, lval *a) {
	// check two arguments, each of which are Q-expressions
	LASSERT_NUM("\\", a, 2);
	LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
	LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

	// check first Q-expression contains only symbols
	for(int i = 0; i < a->cells[0]->count; i++) {
		LASSERT(a, (a->cells[0]->cells[i]->type == LVAL_SYM),
			"Cannot define non-symbol. Got %s, expected %s.",
			ltype_name(a->cells[0]->cells[i]->type),
			ltype_name(LVAL_SYM));
	}

	// pop first two arguments and pass them to lval_lambda
	lval *formals = lval_pop(a, 0);
	lval *body = lval_pop(a, 0);
	lval_del(a);

	return lval_lambda(formals, body);
}

lval *
builtin_list(lenv *e, lval *a) {
	a->type = LVAL_QEXPR;
	return a;
}

lval *
builtin_eval(lenv *e, lval *a) {
	LASSERT_NUM("eval", a, 1);
	LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

	lval *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

lval *
builtin_join(lenv *e, lval *a) {
	for (int i = 0; i < a->count; i++) {
		LASSERT_TYPE("join", a, i, LVAL_QEXPR);
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
builtin_load(lenv *e, lval *a) {
	LASSERT_NUM("load", a, 1);
	LASSERT_TYPE("load", a, 0, LVAL_STR);

	// parse file given by string name
	mpc_result_t r;
	if (!mpc_parse_contents(a->cells[0]->val.str, Lispy, &r)) {
		char *err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);
		lval *err = lval_err("Could not load library %s", err_msg);
		free(err_msg);
		lval_del(a);
		return err;
	}

	// read contents
	lval *expr = lval_read(r.output);
	mpc_ast_delete(r.output);

	// evaluate each expression
	while(expr->count) {
		lval *x = lval_eval(e, lval_pop(expr, 0));

		// lval_print(x);

		// if evaluation produces error, print the error
		if (x->type == LVAL_ERR) { lval_print(x); }
		lval_del(x);
	}

	// delete expressions and arguments
	lval_del(expr);
	lval_del(a);

	// return empty list
	return lval_sexpr();
}

lval *
builtin_print(lenv *e, lval *a) {
	// print each argument followed by a space
	for(int i = 0; i < a->count; i++) {
		lval_print(a->cells[i]); putchar(' ');
	}

	// put a newline and delete arguments
	putchar('\n');
	lval_del(a);
	return lval_sexpr();
}

lval *
builtin_error(lenv *e, lval *a) {
	LASSERT_NUM("error", a, 1);
	LASSERT_TYPE("error", a , 0, LVAL_STR);

	// construct error from first argument
	lval *err = lval_err(a->cells[0]->val.str);

	// delete arguments and return
	lval_del(a);
	return err;
}

int
lval_eq(lval *x, lval *y) {
	// different types never equal
	if (x->type != y->type) {
		return 0;
	}

	switch(x->type) {
		// number values
		case LVAL_NUM:
			return (x->val.num == y->val.num);
		case LVAL_FNUM:
			return (x->val.fnum == y->val.fnum);
		// string values
		case LVAL_ERR:
			return (strcmp(x->val.err, y->val.err) == 0);
		case LVAL_SYM:
			return (strcmp(x->val.sym, y->val.sym) == 0);
		case LVAL_STR:
			return (strcmp(x->val.str, y->val.str) == 0);
		// if builtin, compare function references
		case LVAL_FUN:
			return (x->val.builtin == y->val.builtin);
		// compare formals and body
		case LVAL_LAMBDA:
			return lval_eq(x->val.context->formals, y->val.context->formals) &&
				lval_eq(x->val.context->body, y->val.context->body);
		// if list, compare every element
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			if (x->count != y->count) {
				return 0;
			}
			for(int i = 0; i < x->count; i++) {
				if (!lval_eq(x->cells[i], y->cells[i])) {
					return 0;
				}
			}
			return 1;
		break;
	}
	return 0;
}

lval *
builtin_cmp(lenv *e, lval *a, char *op) {
	LASSERT_NUM(op, a, 2);
	int r;
	if (strcmp(op, "==") == 0) {
		r = lval_eq(a->cells[0], a->cells[1]);
	}
	if (strcmp(op, "!=") == 0) {
		r = !lval_eq(a->cells[0], a->cells[1]);
	}
	lval_del(a);
	return lval_num(r);
}

lval *
builtin_eq(lenv *e, lval *a) {
	return builtin_cmp(e, a, "==");
}

lval *
builtin_ne(lenv *e, lval *a) {
	return builtin_cmp(e, a, "!=");
}

lval *
builtin_if(lenv *e, lval *a) {
	LASSERT_NUM("if", a, 3);
	LASSERT_TYPE("if", a, 0, LVAL_NUM);
	LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
	LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

	// mark both expressions as evaluable
	lval *x;
	a->cells[1]->type = LVAL_SEXPR;
	a->cells[2]->type = LVAL_SEXPR;

	if (a->cells[0]->val.num) {
		// if condition is true, eval first expression
		x = lval_eval(e, lval_pop(a, 1));
	} else {
		// otherwise eval second expression
		x = lval_eval(e, lval_pop(a, 2));
	}

	// delete argument list and return
	lval_del(a);
	return x;
}


lval *
builtin_op(lenv *e, lval *a, char *op) {
	// ensure all operands are numbers
	for(int i = 0; i < a->count; i++) {
		LASSERT_NUM_TYPE(op, a, i);
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

lval *
builtin_ord(lenv *e, lval *a, char *op) {
	LASSERT_NUM(op, a, 2);
	LASSERT_NUM_TYPE(op, a, 0);
	LASSERT_NUM_TYPE(op, a, 1);

	lval *first = a->cells[0];
	lval *second = a->cells[1];
	int r;

	if (first->type == LVAL_FNUM || second->type == LVAL_FNUM) {
		double firstVal, secondVal;
		if (first->type == LVAL_NUM) {
			firstVal = (double) first->val.num;
		} else {
			firstVal = first->val.fnum;
		}
		if (second->type == LVAL_NUM) {
			secondVal = (double) second->val.num;
		} else {
			secondVal = second->val.fnum;
		}
		if (strcmp(op, ">")  == 0) {
			r = (firstVal > secondVal);
	  }
		if (strcmp(op, "<")  == 0) {
			r = (firstVal < secondVal);
		}
		if (strcmp(op, ">=") == 0) {
			r = (firstVal >= secondVal);
		}
		if (strcmp(op, "<=") == 0) {
			r = (firstVal <= secondVal);
		}
	} else {
		int firstVal = first->val.num, secondVal = second->val.num;
		if (strcmp(op, ">")  == 0) {
			r = (firstVal > secondVal);
	  }
		if (strcmp(op, "<")  == 0) {
			r = (firstVal < secondVal);
		}
		if (strcmp(op, ">=") == 0) {
			r = (firstVal >= secondVal);
		}
		if (strcmp(op, "<=") == 0) {
			r = (firstVal <= secondVal);
		}
	}

	lval_del(a);
	return lval_num(r);
}

lval *
builtin_var(lenv *e, lval *a, char *func) {
	LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

	// first arg is symbol list
	lval *syms = a->cells[0];

	// ensure all elements of first list are symbols
	for (int i = 0; i < syms->count; i++) {
		LASSERT(a, (syms->cells[i]->type == LVAL_SYM),
			"Function '%s' cannot define non-symbol. "
			"Got %s, expected %s.",
			func,
			ltype_name(syms->cells[i]->type),
			ltype_name(LVAL_SYM));
	}

	// check correct number of symbols and values
	LASSERT(a, (syms->count == a->count - 1),
		"Function '%s' passed too many arguments for symbols. "
		"Got %i, expected %i.",
		func,
		syms->count,
		a->count - 1);

	for (int i = 0; i < syms->count; i++) {
		// if 'def' define globally, if 'put' define locally
		if (strcmp(func, "def") == 0) {
			lenv_def(e, syms->cells[i], a->cells[i + 1]);
		}
		if (strcmp(func, "put") == 0) {
			lenv_put(e, syms->cells[i], a->cells[i + 1]);
		}
	}

	lval_del(a);
	return lval_sexpr();
}

/* end Builtins */


int
main(int argc, char** argv) {
	Number = mpc_new("number");
	Symbol = mpc_new("symbol");
	String = mpc_new("string");
	Comment = mpc_new("comment");
	Sexpr = mpc_new("sexpr");
	Qexpr = mpc_new("qexpr");
	Expr = mpc_new("expr");
	Lispy = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                    \
			number  : /-?[0-9]+(\\.[0-9]+)?/ ;                 \
			symbol  : /[a-zA-Z0-9_+\\-*\\/%\\\\=<>!&]+/ ;      \
			string  : /\"(\\\\.|[^\"])*\"/ ;                   \
			comment : /;[^\\r\\n]*/ ;                          \
			sexpr   : '(' <expr>* ')' ;                        \
			qexpr   : '{' <expr>* '}' ;                        \
			expr    : <number>  | <symbol> | <string>          \
							| <comment> | <sexpr>  | <qexpr> ;         \
			lispy   : /^/ <expr>* /$/ ;                        \
		",
		Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

	lenv *e = lenv_new();
	lenv_add_builtins(e);

	// read from file
	if (argc >= 2) {
		for(int i = 1; i < argc; i++) {
			lval *args = lval_add(lval_sexpr(), lval_str(argv[i]));

			// pass to builtin load and get result
			lval *x = builtin_load(e, args);

			// if the result is an error, print it
			if (x->type == LVAL_ERR) { lval_print(x); }
			lval_del(x);
		}
	} else { // interactive prompt
		puts("Lispy Version 0.0.0.0.1");
		puts("Press Ctrl+c to exit\n");	

		while(1) {
			char *input = readline("lispy> ");
			add_history(input);

			mpc_result_t r;
			if(mpc_parse("<stdin>", input, Lispy, &r)) {

				lval *x = lval_eval(e, lval_read(r.output));
				lval_println(x);
				lval_del(x);

				mpc_ast_delete(r.output);
			} else {
				mpc_err_print(r.error);
				mpc_err_delete(r.error);
			}

			free(input);
		}
	}

 lenv_del(e);

 mpc_cleanup(8, Number, Symbol, String,
 	Comment, Sexpr, Qexpr, Expr, Lispy);
 return 0;
}