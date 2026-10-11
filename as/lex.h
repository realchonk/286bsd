#define YYSTYPE union yystype
union yystype {
	struct name	*name;
	unsigned long	 constant;
};

extern int lineno, yyleng;
extern YYSTYPE yylval;
extern char yytext[];

extern int yylex(), input();
extern void unput();
