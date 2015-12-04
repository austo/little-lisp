#ifndef __LISPY_H

struct lval;
struct lenv;
struct lcontext;
typedef struct lval lval;
typedef struct lenv lenv;
typedef struct lcontext lcontext;

typedef lval *(*lbuiltin)(lenv *, lval *);

lval *lval_read_num(mpc_ast_t *);
lval *lval_read(mpc_ast_t *);
lval *lval_add(lval*, lval *);
lval *lval_eval_sexpr(lenv *, lval *);
lval *lval_eval(lenv *, lval *);
lval *lval_pop(lval *, int);
lval *lval_take(lval *, int);
lval *lval_join(lval *, lval *);
lval *lval_copy(lval *);

lval *builtin_add(lenv *, lval *);
lval *builtin_sub(lenv *, lval *);
lval *builtin_mul(lenv *, lval *);
lval *builtin_div(lenv *, lval *);
lval *builtin_mod(lenv *, lval *);
lval *builtin_gt(lenv *, lval *);
lval *builtin_lt(lenv *, lval *);
lval *builtin_ge(lenv *, lval *);
lval *builtin_le(lenv *, lval *);
lval *builtin_eq(lenv *, lval *);
lval *builtin_ne(lenv *, lval *);

lval *builtin_if(lenv *, lval *);

lval *builtin_head(lenv *, lval *);
lval *builtin_tail(lenv *, lval *);
lval *builtin_list(lenv *, lval *);
lval *builtin_eval(lenv *, lval *);
lval *builtin_join(lenv *, lval *);
lval *builtin_lambda(lenv *, lval *);

lval *builtin_def(lenv *, lval *);
lval *builtin_put(lenv *, lval *);

lval *builtin_var(lenv *, lval *, char *);
lval *builtin_op(lenv *, lval *, char*);
lval *builtin_ord(lenv *, lval *, char *);
lval *builtin_cmp(lenv *, lval *, char *);

void lval_del(lval *);
void lval_expr_print(lval *, char, char);
void lval_print(lval *);
void lval_println(lval *);

char *ltype_name(int);

lval *lenv_get(lenv *, lval *);
void lenv_put(lenv *, lval *, lval *);
lenv *lenv_copy(lenv *);
void lenv_add_builtin(lenv *, char *, lbuiltin);
void lenv_add_builtins(lenv *);
lval *lval_call(lenv *, lval *, lval *);

int lval_eq(lval *, lval *);

#define __LISPY_H 1
#endif