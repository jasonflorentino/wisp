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
enum { WVAL_NUM, WVAL_ERR };

typedef struct
{
	int type;
	long num;
	int err;
} wval;

wval wval_num(long x)
{
	wval v;
	v.type = WVAL_NUM;
	v.num = x;
	return v;
}

wval wval_err(int x)
{
	wval v;
	v.type = WVAL_ERR;
	v.err = x;
	return v;
}

void wval_print(wval v)
{
	switch (v.type)
	{
		case WVAL_NUM: 
			printf("%li", v.num); break;
		case WVAL_ERR:
			if (v.err == WERR_DIV_ZERO) {
				printf("Error: Division by zero!");
			}
			if (v.err == WERR_BAD_OP)   {
				printf("Error: Invalid operator!");
			}
			if (v.err == WERR_BAD_NUM)  {
				printf("Error: Invalid number!");
			}
			break;
	}
}

void wval_println(wval v)
{
	wval_print(v);
	putchar('\n');
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

wval eval_op(wval x, char* op, wval y)
{
	if (x.type == WVAL_ERR) { return x; }
	if (y.type == WVAL_ERR) { return y; }

	if (strcmp(op, "+") == 0) { return wval_num(x.num + y.num); }
	if (strcmp(op, "-") == 0) { return wval_num(x.num - y.num); }
	if (strcmp(op, "*") == 0) { return wval_num(x.num * y.num); }
	if (strcmp(op, "/") == 0) { 
		return y.num == 0
			? wval_err(WERR_DIV_ZERO)
			: wval_num(x.num / y.num);
	}
	if (strcmp(op, "%") == 0) { return wval_num(x.num % y.num); }
	if (strcmp(op, "^") == 0) { return wval_num(power(x.num, y.num)); }
	return wval_err(WERR_BAD_OP);
}

wval eval(mpc_ast_t* t)
{
	if (strstr(t->tag, "number"))
	{
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
		return errno != ERANGE ? wval_num(x) : wval_err(WERR_BAD_NUM);
	}

	char* op = t->children[1]->contents;
	wval x = eval(t->children[2]);
	
	int i = 3;
	while (strstr(t->children[i]->tag, "expr"))
	{
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}
	return x;
} 

int main(int argc, char** argv)
{
	mpc_parser_t* Number   = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr     = mpc_new("expr");
	mpc_parser_t* Wispy    = mpc_new("wispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                          \
			number   : /-?[0-9]+(\\.[0-9]+)?/ ;                \
			operator : '+' | '-' | '*' | '/' | '%' | '^' ;     \
			expr     : <number> | '(' <operator> <expr>+ ')' ; \
			wispy    : /^/ <operator> <expr>+ /$/ ;            \
		",
		Number, Operator, Expr, Wispy);
	
	puts("\nWispy Version 0.0.0.0.3");
	puts("A lisp-y language by Jason");
	puts("Press Ctrl+C to exit\n");

	while (1)
	{
		char* input = readline("wispy > ");
		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Wispy, &r))
		{
			wval result = eval(r.output);
			wval_println(result);
			mpc_ast_delete(r.output);
		} 
		else 
		{
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		free(input);
	}
	mpc_cleanup(4, Number, Operator, Expr, Wispy);
	return 0;
}
