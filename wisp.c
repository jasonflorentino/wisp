#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt)
{
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	return cpy;
}

void add_history(char* unused) 
{}

#else
#include <editline/readline.h>
#endif

struct wval;
struct wenv;
typedef struct wval wval;
typedef struct wenv wenv;

enum { WVAL_ERR, WVAL_NUM,   WVAL_SYM, 
       WVAL_FUN, WVAL_SEXPR, WVAL_QEXPR };

typedef wval*(*wbuiltin)(wenv*, wval*);

struct wval 
{
	int type;

	long num;
	char* err;
	char* sym;

	wbuiltin builtin;
	wenv* env;
	wval* formals;
	wval* body;

	int count;
	wval** cell;
};


wval* wval_err(char* fmt, ...)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_ERR;

	va_list va;
	va_start(va, fmt);

	v->err = malloc(512);
	vsnprintf(v->err, 511, fmt, va);

	v->err = realloc(v->err, strlen(v->err)+1);

	va_end(va);
	return v;
}

wval* wval_num(long x)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_NUM;
	v->num = x;
	return v;
}

wval* wval_sym(char* s)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

wval* wval_fun(wbuiltin func)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_FUN;
	v->builtin = func;
	return v;
}

wval* wval_sexpr(void)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

wval* wval_qexpr(void)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

wenv* wenv_new(void);

wval* wval_lambda(wval* formals, wval* body)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_FUN;
	v->builtin = NULL;
	v->env = wenv_new();
	v->formals = formals;
	v->body = body;
	return v;
}

void wenv_del(wenv* e);

void wval_del(wval* v)
{
	switch (v->type)
	{
		case WVAL_NUM: break;
		case WVAL_FUN: 
			if (!v->builtin) 
			{
				wenv_del(v->env);
				wval_del(v->formals);
				wval_del(v->body);
			}
			break;
		case WVAL_ERR: free(v->err); break;
		case WVAL_SYM: free(v->sym); break;
		case WVAL_QEXPR:
		case WVAL_SEXPR:
			for (int i = 0; i < v->count; i++) {
				wval_del(v->cell[i]);
			}
			free(v->cell);
			break;
	}
	free(v);
}

wenv* wenv_copy(wenv* e);

wval* wval_copy(wval* v)
{
	wval* x = malloc(sizeof(wval));
	x->type = v->type;

	switch (v->type)
	{
		case WVAL_FUN: 
			if (v->builtin) {
				x->builtin = v->builtin;
			} else {
				x->builtin = NULL;
				x->env = wenv_copy(v->env);
				x->formals = wval_copy(v->formals);
				x->body = wval_copy(v->body);
			}
			break;
		case WVAL_NUM: x->num = v->num; break;
		case WVAL_ERR:
			x->err = malloc(strlen(v->err) + 1);
			strcpy(x->err, v->err); break;
		case WVAL_SYM:
			x->sym = malloc(strlen(v->sym) + 1);
			strcpy(x->sym, v->sym); break;
		case WVAL_SEXPR:
		case WVAL_QEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(wval*) * x->count);
			for (int i = 0; i < x->count; i++) {
				x->cell[i] = wval_copy(v->cell[i]);
			}
			break;
	}
	return x;
}

wval* wval_add(wval* v, wval* x)
{
	v->count++;
	v->cell = realloc(v->cell, sizeof(wval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

/** 
 * `wval_pop` extracts a single element from an S-Expression at
 * index `i` and shifts the rest of the list backward so that it
 * no longer contains that `wval*`
 */
wval* wval_pop(wval* v, int i)
{
	wval* x = v->cell[i];
	memmove(&v->cell[i], 
		&v->cell[i+1],
		sizeof(wval*) * (v->count-i-1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(wval*) * v->count);
	return x;
}

/**
 * `wval_take` extracts a single element from an S-Expression at
 * index `i` and deletes the list it extracted the element from.
 */
wval* wval_take(wval* v, int i)
{
	wval* x = wval_pop(v, i);
	wval_del(v);
	return x;
}

/**
 * `wval_join` pops each item from `y`, adds it to `x`
 * then returns `x`.
 */
wval* wval_join(wval* x, wval* y)
{
	while (y->count) {
		x = wval_add(x, wval_pop(y, 0));
	}

	wval_del(y);
	return x;
}

void wval_print(wval* v);
void wval_expr_print(wval* v, char open, char close)
{
	putchar(open);
	for (int i = 0; i < v->count; i++)
	{
		wval_print(v->cell[i]);
		if (i != (v->count-1)) { 
			putchar(' '); 
		}
	}
	putchar(close);
}

void wval_print(wval* v)
{
	switch (v->type)
	{
		case WVAL_NUM:   printf("%li", v->num); break;
		case WVAL_ERR:   printf("Error: %s", v->err); break;
		case WVAL_FUN:
			if (v->builtin) {
				printf("<builtin>");
			} else {
				printf("(\\ "); wval_print(v->formals);
				putchar(' '); wval_print(v->body); putchar(')');
			}
			break;
		case WVAL_SYM:	 printf("%s", v->sym); break;
		case WVAL_SEXPR: wval_expr_print(v, '(', ')'); break;
		case WVAL_QEXPR: wval_expr_print(v, '{', '}'); break;
	}
}

void wval_println(wval* v)
{
	printf("    <~  ");
	wval_print(v);
	putchar('\n');
}

char* wtype_name(int t)
{
	switch(t)
	{
		case WVAL_FUN: return "Function";
		case WVAL_NUM: return "Number";
		case WVAL_ERR: return "Error";
		case WVAL_SYM: return "Symbol";
		case WVAL_SEXPR: return "S-Expression";
		case WVAL_QEXPR: return "Q-Expression";
		default: return "Unknown";
	}
}

/**
 * Wisp Environment
 */

struct wenv
{
	wenv* par;
	int count;
	char** syms;
	wval** vals;
};

wenv* wenv_new(void)
{
	wenv* e = malloc(sizeof(wenv));
	e->par = NULL;
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void wenv_del(wenv* e)
{
	for (int i = 0; i < e->count; i++)
	{
		free(e->syms[i]);
		wval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

wval* wenv_get(wenv* e, wval* k)
{
	for (int i = 0; i < e->count; i++) {
		if (strcmp(e->syms[i], k->sym) == 0) {
			return wval_copy(e->vals[i]);
		}
	}
	
	if (e->par) {
		return wenv_get(e->par, k);
	} else {
		return wval_err("Unbound symbol `%s`", k->sym);
	}
}

wenv* wenv_copy(wenv* e)
{
	wenv* n = malloc(sizeof(wenv));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char*) * n->count);
	n->vals = malloc(sizeof(wval*) * n->count);
	for (int i = 0; i < e->count; i++)
	{
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = wval_copy(e->vals[i]);
	}
	return n;
}

/* For variable definition in loval env */
void wenv_put(wenv* e, wval* k, wval* v)
{
	for (int i = 0; i < e->count; i++) {
		if (strcmp(e->syms[i], k->sym) == 0)
		{
			wval_del(e->vals[i]);
			e->vals[i] = wval_copy(v);
			return;
		}
	}
	
	e->count++;
	e->vals = realloc(e->vals, sizeof(wval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	e->vals[e->count-1] = wval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);
}

/* For variable definition in global env */
void wenv_def(wenv* e, wval* k, wval* v)
{
	while (e->par) { e = e->par; }
	wenv_put(e, k, v);
}

#define WASSERT(args, cond, fmt, ...) \
	if (!(cond)) { \
		wval* err = wval_err(fmt, ##__VA_ARGS__); \
		wval_del(args); \
		return err; \
	}

#define WASSERT_TYPE(func, args, index, expect) \
	WASSERT(args, args->cell[index]->type == expect, \
		"Function '%s' passed incorrect type for argument %i. Got %s, Expected %s", \
		func, index, wtype_name(args->cell[index]->type), wtype_name(expect))

#define WASSERT_NUM(func, args, num) \
	WASSERT(args, args->count == num, \
		"Funcion '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
		func, args->count, num)

#define WASSERT_NOT_EMPTY(func, args, index) \
	WASSERT(args, args->cell[index]->count != 0, \
		"Function '%s' passed {} for argument %i.", func, index);

wval* wval_eval(wenv* e, wval* v);

/**
 * `head` takes a Q-Expression and returns a Q-Expression with only
 * the first element.
 */
wval* builtin_head(wenv* e, wval* a)
{
	WASSERT_NUM("head", a, 1);
	WASSERT_TYPE("head", a, 0, WVAL_QEXPR);
	WASSERT_NOT_EMPTY("head", a, 0);
	
	wval* v = wval_take(a, 0);
	while (v->count > 1) { wval_del(wval_pop(v, 1)); }
	return v;
}

/**
 * `tail` takes a Q-Expression and returns a Q-Expression with
 * the first element removed.
 */
wval* builtin_tail(wenv* e, wval* a)
{
	WASSERT_NUM("tail", a, 1);
	WASSERT_TYPE("tail", a, 0, WVAL_QEXPR);
	WASSERT_NOT_EMPTY("tail", a, 0);

	wval* v = wval_take(a, 0);
	wval_del(wval_pop(v, 0));
	return v;
}

/**
 * `list` takes an S-Expression and returns it as a Q-Expression.
 */
wval* builtin_list(wenv* e, wval* a)
{
	a->type = WVAL_QEXPR;
	return a;
}

/**
 * `builtin_join` takes one or more Q-Expressions and returns
 * a Q-Expression of them joined together.
 */
wval* builtin_join(wenv* e, wval* a)
{
	for (int i = 0; i < a->count; i++) {
		WASSERT_TYPE("join", a, i, WVAL_QEXPR); 
	}
	
	wval* x = wval_pop(a, 0);
	while (a->count) {
		x = wval_join(x, wval_pop(a, 0));
	}
	
	wval_del(a);
	return x;
}

/**
 * `eval` takes a single Q-Expression, converts it to an S-Expression
 * and evaluates it using `wval_eval`.
 */
wval* builtin_eval(wenv* e, wval* a)
{
	WASSERT_NUM("eval", a, 1);
	WASSERT_TYPE("eval", a, 0, WVAL_QEXPR);

	wval* x = wval_take(a, 0);
	x->type = WVAL_SEXPR;
	return wval_eval(e, x);
}

/**
 * `builtin_op` handles evaluation. First ensure all arguments are numbers
 * then pop first argument. If there are no more sub-expressions and the
 * operator is subtraction, perform unary negation. If there are more
 * arguments, continue popping and performing necessary operations.
 * If no errors, delete arguments and return new expression.
 */
wval* builtin_op(wenv* e, wval* a, char* op)
{
	for (int i = 0; i < a->count; i++) {
		WASSERT_TYPE(op, a, i, WVAL_NUM);	
	}
	
	wval* x = wval_pop(a, 0);
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}
	while (a->count > 0)
	{
		wval* y = wval_pop(a, 0);
		if (strcmp(op, "+") == 0) { x->num += y->num; }
		if (strcmp(op, "-") == 0) { x->num -= y->num; }
		if (strcmp(op, "*") == 0) { x->num *= y->num; }
		if (strcmp(op, "%") == 0) { x->num %= y->num; }
		if (strcmp(op, "/") == 0) { 
			if (y->num == 0) 
			{
				wval_del(x); wval_del(y);
				x = wval_err("Division by zero!");
				break;
			}
			x->num /= y->num;
		}
		wval_del(y);
	}
	wval_del(a);
	return x;
}

wval* builtin_add(wenv* e, wval* a) {
	return builtin_op(e, a, "+");
}

wval* builtin_sub(wenv* e, wval* a) {
	return builtin_op(e, a, "-");
}

wval* builtin_mul(wenv* e, wval* a) {
	return builtin_op(e, a, "*");
}

wval* builtin_div(wenv* e, wval* a) {
	return builtin_op(e, a, "/");
}

wval* builtin_var(wenv* e, wval* a, char* func)
{
	WASSERT_TYPE("def", a, 0, WVAL_QEXPR);

	wval* syms = a->cell[0];
	for (int i = 0; i < syms->count; i++)
	{
		WASSERT(a, (syms->cell[i]->type == WVAL_SYM),
			"Function '%s' cannot define non-symbol! ",
			"Got %s, Expected %s.", func,
			wtype_name(syms->cell[i]->type), 
			wtype_name(WVAL_SYM));
	}

	WASSERT(a, (syms->count == a->count-1),
		"Function '%s' passed incorrect number of arguments for symbols. "
		"Got %i, Expected %i.",
		func, syms->count, a->count-1);

	for (int i = 0; i < syms->count; i++) 
	{
		if (strcmp(func, "def") == 0) {
			wenv_def(e, syms->cell[i], a->cell[i+1]);
		}

		if (strcmp(func, "=")   == 0) {
			wenv_put(e, syms->cell[i], a->cell[i+1]);
		}
	}

	wval_del(a);
	return wval_sexpr();
}

wval* builtin_def(wenv* e, wval* a)
{
	return builtin_var(e, a, "def");
}

wval* builtin_put(wenv* e, wval* a)
{
	return builtin_var(e, a, "=");
}

wval* builtin_lambda(wenv* e, wval* a)
{
	WASSERT_NUM("\\", a, 2);
	WASSERT_TYPE("\\", a, 0, WVAL_QEXPR);
	WASSERT_TYPE("\\", a, 1, WVAL_QEXPR);

	for (int i = 0; i < a->cell[0]->count; i++)
	{
		WASSERT(a, (a->cell[0]->cell[i]->type == WVAL_SYM),
			"Cannot define non-symbol. Got %s, Expected %s",
			wtype_name(a->cell[0]->cell[i]->type), wtype_name(WVAL_SYM));
	}
	
	wval* formals =  wval_pop(a, 0);
	wval* body = wval_pop(a, 0);
	wval_del(a);

	return wval_lambda(formals, body);
} 

void wenv_add_builtin(wenv* e, char* name, wbuiltin func)
{
	wval* k = wval_sym(name);
	wval* v = wval_fun(func);
	wenv_put(e, k, v);
	wval_del(k);
	wval_del(v);
}

void wenv_add_builtins(wenv* e)
{
	wenv_add_builtin(e, "list", builtin_list);
	wenv_add_builtin(e, "head", builtin_head);
	wenv_add_builtin(e, "tail", builtin_tail);
	wenv_add_builtin(e, "eval", builtin_eval);
	wenv_add_builtin(e, "join", builtin_join);
	wenv_add_builtin(e, "def",  builtin_def);
	wenv_add_builtin(e, "=",    builtin_put);
	wenv_add_builtin(e, "\\",   builtin_lambda);

	wenv_add_builtin(e, "+", builtin_add);
	wenv_add_builtin(e, "-", builtin_sub);
	wenv_add_builtin(e, "*", builtin_mul);
	wenv_add_builtin(e, "/", builtin_div);
}

wval* wval_call(wenv* e, wval* f, wval* a)
{
	if (f->builtin) { return f->builtin(e, a); }
	
	int given = a->count;
	int total = f->formals->count;

	while (a->count)
	{
		if (f->formals->count == 0)
		{
			wval_del(a); 
			return wval_err(
				"Function passed too many arguments. "
				"Got %i, Expected %i.", given, total);
		}
		
		wval* sym = wval_pop(f->formals, 0);
		
		if (strcmp(sym->sym, "&") == 0)
		{
			if (f->formals->count != 1)
			{
				wval_del(a);
				return wval_err("Function format invalid. "
					"Symbol '&' not followed by single symbol.");
			}
			
			wval* nsym = wval_pop(f->formals, 0);
			wenv_put(f->env, nsym, builtin_list(e, a));
			wval_del(sym); wval_del(nsym);
			break;
		}
		
		wval* val = wval_pop(a, 0);
		wenv_put(f->env, sym, val);
		wval_del(sym); wval_del(val);
	}

	wval_del(a);
	
	if (f->formals->count > 0 &&
		strcmp(f->formals->cell[0]->sym, "&") == 0)
	{
		if (f->formals->count != 2) {
			return wval_err("Function format invalid. "
				"Symbol '&' not followed by single symbol.");
		}
		
		wval_del(wval_pop(f->formals, 0));

		wval* sym = wval_pop(f->formals, 0);
		wval* val = wval_qexpr();

		wenv_put(f->env, sym, val);
		wval_del(sym); wval_del(val);
	}

	if (f->formals->count == 0) {
		f->env->par = e;
		return builtin_eval(
			f->env, wval_add(wval_sexpr(), wval_copy(f->body)));
	} else {
		return wval_copy(f);
	}
}

/**
 * `wval_eval_sexp` takes an `wval*` and transforms it to a new `wval*`
 * First evaluate the children. If there are any errors, return the first
 * error using `wval_take`. If there are no children return it directly.
 * Then if single single expression, return it contained in parenthesis.
 * Else we have a valid expression with more than one child.
 */
wval* wval_eval_sexpr(wenv* e, wval* v)
{
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = wval_eval(e, v->cell[i]);
	}
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == WVAL_ERR) {
			return wval_take(v, i);
		}
	}

	if (v->count == 0) { return v; }
	if (v->count == 1) { return wval_take(v, 0); }
	
	wval* f = wval_pop(v, 0);
	if (f->type != WVAL_FUN)
	{
		wval* err = wval_err(
			"S-Expression starts with incorrect type. "
			"Got %s, Expected %s.",
			wtype_name(f->type), wtype_name(WVAL_FUN));
		wval_del(f);
		wval_del(v);
		return err;
	}

	wval* result = wval_call(e, f, v);
	wval_del(f);
	return result;
}

wval* wval_eval(wenv* e, wval* v)
{
	if (v->type == WVAL_SYM)
	{
		wval* x = wenv_get(e, v);
		wval_del(v);
		return x;
	}
	if (v->type == WVAL_SEXPR) {
		return wval_eval_sexpr(e, v);
	}
	return v;
}

/* Custom exponentiation function */
long power(long base, long exp)
{
	if (exp == 0) {
		return 1;
	} else if (exp % 2) {
		return base * power(base, exp - 1);
	} else {
		long temp = power(base, exp / 2);
		return temp * temp;
	}
}

wval* wval_read_num(mpc_ast_t* t)
{
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ?
		wval_num(x) : wval_err("invalid number");
}

wval* wval_read(mpc_ast_t* t)
{
	if (strstr(t->tag, "number")) { return wval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return wval_sym(t->contents); }

	wval* x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = wval_sexpr(); }
	if (strstr(t->tag, "sexpr"))  { x = wval_sexpr(); }
	if (strstr(t->tag, "qexpr"))  { x = wval_qexpr(); }

	for (int i = 0; i < t->children_num; i++)
	{
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
		x = wval_add(x, wval_read(t->children[i]));
	}
	
	return x;
}

int main(int argc, char** argv)
{
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr  = mpc_new("sexpr");
	mpc_parser_t* Qexpr  = mpc_new("qexpr");
	mpc_parser_t* Expr   = mpc_new("expr");
	mpc_parser_t* Wispy  = mpc_new("wispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                          \
			number : /-?[0-9]+(\\.[0-9]+)?/ ;                  \
			symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        \
			sexpr  : '(' <expr>* ')' ;                         \
			qexpr  : '{' <expr>* '}' ;                         \
			expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
			wispy  : /^/ <expr>* /$/ ;                         \
		",
		Number, Symbol, Sexpr, Qexpr, Expr, Wispy);
	
	puts("\n Wispy Version 0.0.0.0.5");
	puts(" A lisp-y language by Jason");
	puts(" Made reading buildyourownlisp.com by Daniel Holden");
	puts(" Press Ctrl+C to exit\n");

	wenv* e = wenv_new();
	wenv_add_builtins(e);

	while (1)
	{
		char* input = readline("wispy~> ");
		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Wispy, &r))
		{
			wval* x = wval_eval(e, wval_read(r.output));
			wval_println(x);
			wval_del(x);
			mpc_ast_delete(r.output);
		} 
		else 
		{
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		free(input);
	}
	wenv_del(e);
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Wispy);
	return 0;
}
