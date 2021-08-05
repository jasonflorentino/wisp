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

/* Parser declarations */

mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Wispy;

/* Forward declarations */

struct wval;
struct wenv;
typedef struct wval wval;
typedef struct wenv wenv;

/* Wisp Value */

enum { WVAL_ERR, WVAL_NUM,   WVAL_SYM, WVAL_STR, 
       WVAL_FUN, WVAL_SEXPR, WVAL_QEXPR };

typedef wval*(*wbuiltin)(wenv*, wval*);

struct wval 
{
	int type;
	
	/* Basic */
	long num;
	char* err;
	char* sym;
	char* str;

	/* Function */
	wbuiltin builtin;
	wenv* env;
	wval* formals;
	wval* body;

	/* Expression */
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

wval* wval_str(char *s)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_STR;
	v->str = malloc(strlen(s) + 1);
	strcpy(v->str, s);
	return v;
}

wval* wval_builtin(wbuiltin func)
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
		case WVAL_STR: free(v->str); break;
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
		case WVAL_STR:
			x->str = malloc(strlen(v->str) +1);
			strcpy(x->str, v->str); break;
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

wval* wval_take(wval* v, int i)
{
	wval* x = wval_pop(v, i);
	wval_del(v);
	return x;
}

wval* wval_join(wval* x, wval* y)
{
	while (y->count) {
		x = wval_add(x, wval_pop(y, 0));
	}

	wval_del(y);
	return x;
}

int wval_eq(wval* x, wval* y)
{
	if (x->type != y->type) { return 0; }

	switch (x->type)
	{
		case WVAL_NUM: return (x->num == y->num);
		case WVAL_ERR: return (strcmp(x->err, y->err) == 0);
		case WVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
		case WVAL_STR: return (strcmp(x->str, y->str) == 0);
		case WVAL_FUN:
			if (x->builtin || y->builtin) {
				return x->builtin == y->builtin;
			} else {
				return wval_eq(x->formals, y->formals)
				    && wval_eq(x->body, y->body);
			}
		case WVAL_QEXPR:
		case WVAL_SEXPR:
			if (x->count != y->count) { return 0; }
			for (int i = 0; i < x->count; i++) {
				if (!wval_eq(x->cell[i], y->cell[i])) { return 0; }
			}
			return 1;
		break;
	}
	return 0;
}

void wval_print_str(wval* v)
{
	char* escaped = malloc(strlen(v->str) + 1);
	strcpy(escaped, v->str);
	escaped = mpcf_escape(escaped);
	printf("\"%s\"", escaped);
	free(escaped);
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
		case WVAL_SYM:	 printf("%s", v->sym); break;
		case WVAL_STR:   wval_print_str(v); break;
		case WVAL_SEXPR: wval_expr_print(v, '(', ')'); break;
		case WVAL_QEXPR: wval_expr_print(v, '{', '}'); break;
		case WVAL_FUN:
			if (v->builtin) {
				printf("<builtin>");
			} else {
				printf("(\\ "); wval_print(v->formals);
				putchar(' '); wval_print(v->body); putchar(')');
			}
			break;
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
		case WVAL_FUN:   return "Function";
		case WVAL_NUM:   return "Number";
		case WVAL_ERR:   return "Error";
		case WVAL_SYM:   return "Symbol";
		case WVAL_STR:   return "String";
		case WVAL_SEXPR: return "S-Expression";
		case WVAL_QEXPR: return "Q-Expression";
		default:         return "Unknown";
	}
}

/* Wisp Environment */

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

/* Builtins */

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

wval* builtin_head(wenv* e, wval* a)
{
	WASSERT_NUM("head", a, 1);
	WASSERT_TYPE("head", a, 0, WVAL_QEXPR);
	WASSERT_NOT_EMPTY("head", a, 0);
	
	wval* v = wval_take(a, 0);
	while (v->count > 1) { wval_del(wval_pop(v, 1)); }
	return v;
}

wval* builtin_tail(wenv* e, wval* a)
{
	WASSERT_NUM("tail", a, 1);
	WASSERT_TYPE("tail", a, 0, WVAL_QEXPR);
	WASSERT_NOT_EMPTY("tail", a, 0);

	wval* v = wval_take(a, 0);
	wval_del(wval_pop(v, 0));
	return v;
}

wval* builtin_list(wenv* e, wval* a)
{
	a->type = WVAL_QEXPR;
	return a;
}

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

wval* builtin_eval(wenv* e, wval* a)
{
	WASSERT_NUM("eval", a, 1);
	WASSERT_TYPE("eval", a, 0, WVAL_QEXPR);

	wval* x = wval_take(a, 0);
	x->type = WVAL_SEXPR;
	return wval_eval(e, x);
}

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

wval* builtin_add(wenv* e, wval* a) { return builtin_op(e, a, "+"); }
wval* builtin_sub(wenv* e, wval* a) { return builtin_op(e, a, "-"); }
wval* builtin_mul(wenv* e, wval* a) { return builtin_op(e, a, "*"); }
wval* builtin_div(wenv* e, wval* a) { return builtin_op(e, a, "/"); }

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

wval* builtin_def(wenv* e, wval* a) { return builtin_var(e, a, "def"); }
wval* builtin_put(wenv* e, wval* a) { return builtin_var(e, a, "="); }

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

wval* builtin_ord(wenv* e, wval* a, char* op)
{
	WASSERT_NUM(op, a, 2);
	WASSERT_TYPE(op, a, 0, WVAL_NUM);
	WASSERT_TYPE(op, a, 1, WVAL_NUM);

	int r;
	if (strcmp(op, ">")  == 0) { r = (a->cell[0]->num >  a->cell[1]->num); }
	if (strcmp(op, "<")  == 0) { r = (a->cell[0]->num <  a->cell[1]->num); }
	if (strcmp(op, ">=") == 0) { r = (a->cell[0]->num >= a->cell[1]->num); }
	if (strcmp(op, "<=") == 0) { r = (a->cell[0]->num <= a->cell[1]->num); }
	wval_del(a);
	return wval_num(r);
}

wval* builtin_gt(wenv* e, wval* a) { return builtin_ord(e, a, ">");  }
wval* builtin_lt(wenv* e, wval* a) { return builtin_ord(e, a, "<");  }
wval* builtin_ge(wenv* e, wval* a) { return builtin_ord(e, a, ">="); }
wval* builtin_le(wenv* e, wval* a) { return builtin_ord(e, a, "<="); }

wval* builtin_cmp(wenv* e, wval* a, char* op)
{
	WASSERT_NUM(op, a, 2);
	int r;
	if (strcmp(op, "==") == 0) { r =  wval_eq(a->cell[0], a->cell[1]); }
	if (strcmp(op, "!=") == 0) { r = !wval_eq(a->cell[0], a->cell[1]); }
	wval_del(a);
	return wval_num(r);
}

wval* builtin_eq(wenv* e, wval* a) { return builtin_cmp(e, a, "=="); }
wval* builtin_ne(wenv* e, wval* a) { return builtin_cmp(e, a, "!="); }

wval* builtin_if(wenv* e, wval* a)
{
	WASSERT_NUM("if",  a, 3);
	WASSERT_TYPE("if", a, 0, WVAL_NUM);
	WASSERT_TYPE("if", a, 1, WVAL_QEXPR);
	WASSERT_TYPE("if", a, 2, WVAL_QEXPR);

	wval* x;
	a->cell[1]->type = WVAL_SEXPR;
	a->cell[2]->type = WVAL_SEXPR;

	if (a->cell[0]->num) {
		x = wval_eval(e, wval_pop(a, 1));
	} else {
		x = wval_eval(e, wval_pop(a, 2));
	}
	
	wval_del(a);
	return x;
}

wval* wval_read(mpc_ast_t* t);

wval* builtin_load(wenv* e, wval* a)
{
	WASSERT_NUM("load", a, 1);
	WASSERT_TYPE("load", a, 0, WVAL_STR);

	mpc_result_t r;
	if (mpc_parse_contents(a->cell[0]->str, Wispy, &r))
	{
		wval* expr = wval_read(r.output);
		mpc_ast_delete(r.output);

		while (expr->count)
		{
			wval* x = wval_eval(e, wval_pop(expr, 0));
			if (x->type == WVAL_ERR) { wval_println(x); }
			wval_del(x);
		}

		wval_del(expr);
		wval_del(a);
		
		return wval_sexpr();
	}
	else
	{
		char* err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);

		wval* err = wval_err("Could not load Library %s", err_msg);
		free(err_msg);
		wval_del(a);
	
		return err;
	}
}

wval* builtin_print(wenv* e, wval* a)
{
	for (int i = 0; i < a->count; i++) {
		wval_print(a->cell[i]); putchar(' ');
	}
	putchar('\n');
	wval_del(a);

	return wval_sexpr();
}

wval* builtin_error(wenv* e, wval* a)
{
	WASSERT_NUM("error", a, 1);
	WASSERT_TYPE("error", a, 0, WVAL_STR);

	wval* err = wval_err(a->cell[0]->str);
	
	wval_del(a);
	return err;
}

void wenv_add_builtin(wenv* e, char* name, wbuiltin func)
{
	wval* k = wval_sym(name);
	wval* v = wval_builtin(func);
	wenv_put(e, k, v);
	wval_del(k);
	wval_del(v);
}

void wenv_add_builtins(wenv* e)
{
	/* Variable functions */
	wenv_add_builtin(e, "def", builtin_def);
	wenv_add_builtin(e, "=",   builtin_put);
	wenv_add_builtin(e, "\\",  builtin_lambda);

	/* List functions */
	wenv_add_builtin(e, "list", builtin_list);
	wenv_add_builtin(e, "head", builtin_head);
	wenv_add_builtin(e, "tail", builtin_tail);
	wenv_add_builtin(e, "eval", builtin_eval);
	wenv_add_builtin(e, "join", builtin_join);
	
	/* Math functions */
	wenv_add_builtin(e, "+", builtin_add);
	wenv_add_builtin(e, "-", builtin_sub);
	wenv_add_builtin(e, "*", builtin_mul);
	wenv_add_builtin(e, "/", builtin_div);
	
	/* Comparison functions */
	wenv_add_builtin(e, "if", builtin_if);
	wenv_add_builtin(e, "==", builtin_eq);
	wenv_add_builtin(e, "!=", builtin_ne);
	wenv_add_builtin(e, ">",  builtin_gt);
	wenv_add_builtin(e, "<",  builtin_lt);
	wenv_add_builtin(e, ">=", builtin_ge);
	wenv_add_builtin(e, "<=", builtin_le);
	
	/* String functions */
	wenv_add_builtin(e, "load",  builtin_load);
	wenv_add_builtin(e, "error", builtin_error);
	wenv_add_builtin(e, "print", builtin_print);
}

/* Evaluation */

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
	if (v->count == 1) { return wval_eval(e, wval_take(v, 0)); }
	
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

/* Reading */

wval* wval_read_num(mpc_ast_t* t)
{
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ?
		wval_num(x) : wval_err("Invalid number");
}

wval* wval_read_str(mpc_ast_t* t)
{
	t->contents[strlen(t->contents)-1] = '\0';
	char* unescaped = malloc(strlen(t->contents+1)+1);
	strcpy(unescaped, t->contents+1);
	unescaped = mpcf_unescape(unescaped);
	wval* str = wval_str(unescaped);
	free(unescaped);
	return str;
}

wval* wval_read(mpc_ast_t* t)
{
	if (strstr(t->tag, "number")) { return wval_read_num(t); }
	if (strstr(t->tag, "string")) { return wval_read_str(t); }
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
		if (strstr(t->children[i]->tag, "comment")) { continue; }
		x = wval_add(x, wval_read(t->children[i]));
	}
	
	return x;
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

/* Main */

int main(int argc, char** argv)
{
	Number  = mpc_new("number");
	Symbol  = mpc_new("symbol");
	String  = mpc_new("string");
	Comment = mpc_new("comment");
	Sexpr   = mpc_new("sexpr");
	Qexpr   = mpc_new("qexpr");
	Expr    = mpc_new("expr");
	Wispy   = mpc_new("wispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                    \
			number  : /-?[0-9]+(\\.[0-9]+)?/ ;           \
			symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
			string  : /\"(\\\\.|[^\"])*\"/ ;             \
			comment : /;[^\\r\\n]*/ ;                    \
			sexpr   : '(' <expr>* ')' ;                  \
			qexpr   : '{' <expr>* '}' ;                  \
			expr    : <number>  | <symbol> | <string>    \
			        | <comment> | <sexpr>  | <qexpr> ;   \
			wispy   : /^/ <expr>* /$/ ;                  \
		",
		Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Wispy);

	wenv* e = wenv_new();
	wenv_add_builtins(e);

	if (argc == 1)
	{		
		puts("\n Wisp Version 0.0.6");
		puts(" A lisp-y language by Jason");
		puts(" Made reading buildyourownlisp.com by Daniel Holden");
		puts(" Press Ctrl+C to exit\n");
	
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
	}

	if (argc >= 2)
	{
		for (int i = 1; i < argc; i++)
		{
			wval* args = wval_add(wval_sexpr(), wval_str(argv[i]));
			wval* x = builtin_load(e, args);
			if (x->type == WVAL_ERR) { wval_println(x); }
			wval_del(x);
		}
	}

	wenv_del(e);

	mpc_cleanup(8,
		Number, Symbol, String, Comment, 
		Sexpr,  Qexpr,  Expr,   Wispy);

	return 0;
}
