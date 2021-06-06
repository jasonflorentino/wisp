#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

/* If compiling on Windows */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt)
{
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#endif

/* Enumeration of possible error types */
enum { WERR_DIV_ZERO, WERR_BAD_OP, WERR_BAD_NUM };

/* Enumeration of possible wval types */
enum { WVAL_NUM, WVAL_ERR };

/* wval struct */
typedef struct
{
	int type;
	long num;
	int err;
} wval;

/* Create a number type wval */
wval wval_num(long x)
{
	wval v;
	v.type = WVAL_NUM;
	v.num = x;
	return v;
}

/* Create a error type wval */
wval wval_err(int x)
{
	wval v;
	v.type = WVAL_ERR;
	v.err = x;
	return v;
}

/* Print a "wval" */
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

/* Print a "wval" with a newline */
void wval_println(wval v)
{
	wval_print(v);
	putchar('\n');
}

/* Exponentiation function */
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

/* Use operator string to see which operation to do */
wval eval_op(wval x, char* op, wval y)
{
	/* If either value is an error return it */
	if (x.type == WVAL_ERR) { return x; }
	if (y.type == WVAL_ERR) { return y; }

	/* Otherwise do math on the number values */
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

/* Evaluate parsed input */
wval eval(mpc_ast_t* t)
{
	/* If tagged as number return it directly. */
	if (strstr(t->tag, "number"))
	{
		/* Check if error in conversion */
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
		return errno != ERANGE ? wval_num(x) : wval_err(WERR_BAD_NUM);
	}

	/* The operator is always second child */
	char* op = t->children[1]->contents;
	
	/* Store result of third child in `x` */
	wval x = eval(t->children[2]);
	
	/* Iterate over remaining children and combine */
	int i = 3;
	while (strstr(t->children[i]->tag, "expr"))
	{
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}
	return x;
} 

/* Main function */
int main(int argc, char** argv)
{
	/* Create parsers */
	mpc_parser_t* Number   = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr     = mpc_new("expr");
	mpc_parser_t* Wispy    = mpc_new("wispy");

	/* Define parsers */
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                          \
			number   : /-?[0-9]+(\\.[0-9]+)?/ ;                \
			operator : '+' | '-' | '*' | '/' | '%' | '^' ;     \
			expr     : <number> | '(' <operator> <expr>+ ')' ; \
			wispy    : /^/ <operator> <expr>+ /$/ ;            \
		",
		Number, Operator, Expr, Wispy);
	
	/* Print startup message */
	puts("\nWispy Version 0.0.0.0.1");
	puts("A lisp-y language by Jason");
	puts("Press Ctrl+C to exit\n");

	/* Main loop */
	while (1)
	{
		/* Print prompt */
		char* input = readline("wispy > ");
		add_history(input);

		/* Try to parse user unput */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Wispy, &r))
		{
			/* Evaluate parsed input and print result */
			wval result = eval(r.output);
			wval_println(result);
			mpc_ast_delete(r.output);
		} 
		else 
		{
			/* Otherwise print the error */
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}
	
	/* Undefine and Delete parsers */
	mpc_cleanup(4, Number, Operator, Expr, Wispy);

	return 0;
}
