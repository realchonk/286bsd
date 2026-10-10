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
		| expression indices
		| '$' expression
		| '$' expression indices
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

indices		: index
		| indices index
		;

index		: '(' register ')'
		;
