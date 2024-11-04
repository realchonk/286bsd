%{

%}

%token INTEGER

%%

expr	: add
     	;

add	: add '+' mult
    	| add '-' mult
	| mult
	;

mult	: mult '*' atom
     	| mult '/' atom
	| atom
	;

atom	: INTEGER
     	;

%%
