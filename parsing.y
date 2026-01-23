#include <stdio.h>
#build grammar and AST tree this week
#do semantic analysis

{%
extern int yylex();
extern int yylex_destroy();
extern int yywrap();
int yyerror(char *);
extern FILE *yyin;
%}

%token NAME NUM
%start expr


%%
expr :     term '+'  term
		 | term '-'	 term
		 | term '*'	 term
		 | term '/' term
		 | term {$$ = $1
term : NUM
type: INT
declaration: type NAME ';'
statement:
block:

// describing more grammar rules

%%

int yyerror(char *s){
	fprintf(stderr,"%s\n", s);
	return 0;
}

int main(int argc, char* argv[]){

}

