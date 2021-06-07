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

enum { WERR_DIV_ZERO, WERR_BAD_OP, WERR_BAD_NUM };
enum { WVAL_ERR, WVAL_NUM, WVAL_SYM, WVAL_SYMEXPR };

typedef struct wval
{
	int type;
	long num;
	char* err;
	char* sym;
	int count;
	struct wval** cell;
} wval;

wval* wval_num(long x)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_NUM;
	v->num = x;
	return v;
}

wval* wval_err(char* m)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);
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

wval* wval_symexpr(void)
{
	wval* v = malloc(sizeof(wval));
	v->type = WVAL_SYMEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

void wval_del(wval* v)
{
	switch (v->type)
	{
		case WVAL_NUM: break;
		case WVAL_ERR: free(v->err); break;
		case WVAL_SYM: free(v->sym); break;
		case WVAL_SYMEXPR:
			for (int i = 0; i < v->count; i++) {
				wval_del(v->cell[i]);
			}
			free(v->cell);
			break;
	}
	free(v);
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
		case WVAL_NUM:     printf("%li", v->num); break;
		case WVAL_ERR:     printf("Error: %s", v->err); break;
		case WVAL_SYM:	   printf("%s", v->sym); break;
		case WVAL_SYMEXPR: wval_expr_print(v, '(', ')'); break;
	}
}

void wval_println(wval* v)
{
	printf("    <~  ");
	wval_print(v);
	putchar('\n');
}

/**
 * `builtin_op` handles evaluation. First ensure all arguments are numbers
 * then pop first argument. If there are no more sub-expressions and the
 * operator is subtraction, perform unary negation. If there are more
 * arguments, continue popping and performing necessary operations.
 * If no errors, delete arguments and return new expression.
 */
wval* builtin_op(wval* a, char* op)
{
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != WVAL_NUM) {
			wval_del(a);
			return wval_err("Cannot operate on non-number!");
		}
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

/**
 * `wval_eval_symexp` takes an `wval*` and transforms it to a new `wval*`
 * First evaluate the children. If there are any errors, return the first
 * error using `wval_take`. If there are no children return it directly.
 * Then if single single expression, return it contained in parenthesis.
 * Else we have a valid expression with more than one child.
 */
wval* wval_eval(wval* v);
wval* wval_eval_symexpr(wval* v)
{
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = wval_eval(v->cell[i]);
	}
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == WVAL_ERR) {
			return wval_take(v, i);
		}
	}

	if (v->count == 0) { return v; }
	if (v->count == 1) { return wval_take(v, 0); }
	
	wval* f = wval_pop(v, 0);
	if (f->type != WVAL_SYM)
	{
		wval_del(f);
		wval_del(v);
		return wval_err("S-Expression does not start with a symbol!");
	}

	wval* result = builtin_op(v, f->sym);
	wval_del(f);
	return result;
}

wval* wval_eval(wval* v)
{
	if (v->type == WVAL_SYMEXPR) {
		return wval_eval_symexpr(v);
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
	if (strcmp(t->tag, ">") == 0)  { x = wval_symexpr(); }
	if (strstr(t->tag, "symexpr")) { x = wval_symexpr(); }

	for (int i = 0; i < t->children_num; i++)
	{
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
		x = wval_add(x, wval_read(t->children[i]));
	}
	
	return x;
}

int main(int argc, char** argv)
{
	mpc_parser_t* Number  = mpc_new("number");
	mpc_parser_t* Symbol  = mpc_new("symbol");
	mpc_parser_t* Symexpr = mpc_new("symexpr");
	mpc_parser_t* Expr    = mpc_new("expr");
	mpc_parser_t* Wispy   = mpc_new("wispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                     \
			number  : /-?[0-9]+(\\.[0-9]+)?/ ;            \
			symbol  : '+' | '-' | '*' | '/' | '%' | '^' ; \
			symexpr : '(' <expr>* ')' ;                   \
			expr    : <number> | <symbol> | <symexpr>  ;  \
			wispy   : /^/ <expr>* /$/ ;                   \
		",
		Number, Symbol, Symexpr, Expr, Wispy);
	
	puts("\nWispy Version 0.0.0.0.4");
	puts("A lisp-y language by Jason");
	puts("Press Ctrl+C to exit\n");

	while (1)
	{
		char* input = readline("wispy~> ");
		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Wispy, &r))
		{
			wval* x = wval_eval(wval_read(r.output));
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
	mpc_cleanup(5, Number, Symbol, Symexpr, Expr, Wispy);
	return 0;
}
