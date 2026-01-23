%{ #include <stdio.h>
    extern int yylex();
    extern int yylex_destroy();
    extern int yywrap();
    extern int yyparse(void);
    int yyerror(char *);
    extern FILE *yyin;
%}

%token ID NUM
%token WHILE IF ELSE PRINT INT EXTERN VOID RETURN READ

%start statement_list

%%

assignment : ID '=' expr ;


block: '{' statement_list '}'

statement_list: statement_list statement
              | statement
              ;

statement  : assignment ';'
           | expr ';'
           | block
           ;

expr : expr '+' term
     | expr '-' term
     | term
     ;

term   : term '*' factor
       | term '/' factor
       | factor
       ;

factor : ID
     | NUM
     | '(' expr ')'
     ;

%%

int yyerror(char *s){
	fprintf(stderr,"%s\n", s);
	return 0;
}

int main(int argc, char* argv[]){
		if (argc == 2){
			yyin = fopen(argv[1], "r");
			if (yyin == NULL) {
				fprintf(stderr, "File open error\n");
				return 1;
			}
		}
		yyparse();
		if (argc == 2) fclose(yyin);
		yylex_destroy();
		return 0;
}
