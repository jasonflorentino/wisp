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
			operator : '+' | '-' | '*' | '/' | '%' ;           \
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
			/* On success print the AST */
			mpc_ast_print(r.output);
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
