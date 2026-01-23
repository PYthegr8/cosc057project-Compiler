%{ #include <stdio.h>
    extern int yylex();
    extern int yylex_destroy();
    extern int yywrap();
    int yyerror(char *);
    extern FILE *yyin;
%}

%token ID NUM
%token WHILE IF ELSE PRINT INT EXTERN VOID RETURN READ
%start expr

%%
expr : expr '+' term
     | expr '-' term
     | term
     ;

term : ID
     | NUM
     ;

%%

int yyerror(char *s){
	fprintf(stderr,"%s\n", s);
	return 0;
}

int main(int argc, char* argv[]){

}

