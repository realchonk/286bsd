%{
#include "nametab.h"
#include "lex.h"
%}

%token  NAME
%token  CONSTANT

%%

program		:
		| program line
		;

line		: label instruction nl
		| instruction nl
		;

label		: NAME ':'
		;

instruction	:
		| NAME
		| NAME operand
		| NAME operand ',' operand
		| NAME operand ',' operand ',' operand
		;

nl		: ';'
		| '\n'
		;

operand		: register
		| register ':' register
		| expression index indices
		| '$' expression indices
		| '*' expression indices
		;

register	: NAME
		;

expression	: '(' expression ')'
		| expression '+' term
		| expression '-' term
		| term
		;

term		: '+' symbol 
		| '-' symbol
		| symbol
		;
 
symbol		: NAME
		| CONSTANT
		;

indices		:
		| indices index
		;

index		: '(' register ')'
		;
