#include <stdint.h>
#include <stdio.h>

#include "lex.h"

int main()
{
	int tok;

	while (tok = yylex(), tok != 0)
		printf("%3d %s\n", tok, yytext);
}
