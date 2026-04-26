#include <stdint.h>

extern int lineno, yyleng;
extern uint32_t tokval;
extern char yytext[];

enum {
	TOK_EOF		= 0,
	TOK_NAME	= 256,
	TOK_NUM,
};

extern int yylex(), input();
extern void unput();
