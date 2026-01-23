%{  #include <stdio.h>
    #include "ast.h"
    extern int yylex();
    extern int yylex_destroy();
    extern int yywrap();
    extern int yyparse(void);
    int yyerror(const char *);
    extern FILE *yyin;
%}

%union {int ival;
        char *sval;
        astNode *node;
        vector<astNode*> *slist;
}

%token <sval> ID
%token <ival> NUM
%token WHILE IF ELSE PRINT INT EXTERN VOID RETURN READ
%token LE GE EQ NE

%nonassoc IFX
%nonassoc ELSE

%type <node> condition factor term expr
%type <node> assignment declaration return_statement print_statement
%type <node> statement block
%type <slist> statement_list

%start statement_list

%%

assignment : ID '=' expr  { $$ = createAsgn(createVar($1), $3); } ;

declaration: INT ID ';'   { $$ = createDecl($2); } ;

return_statement : RETURN '(' expr ')' ';'  { $$ = createRet($3); } ;

print_statement: PRINT '(' expr ')' ';'  { $$ = createCall("print", $3); } ;

condition : expr '<'  expr  { $$ = createRExpr($1, $3, lt); }
          | expr '>'  expr  { $$ = createRExpr($1, $3, gt); }
          | expr LE   expr  { $$ = createRExpr($1, $3, le); }
          | expr GE   expr  { $$ = createRExpr($1, $3, ge); }
          | expr EQ   expr  { $$ = createRExpr($1, $3, eq); }
          | expr NE   expr  { $$ = createRExpr($1, $3, neq); }
          ;

block: '{' statement_list '}' { $$ = createBlock($2); } ;

statement_list: statement_list statement    {   $1->push_back($2); $$ = $1; }
              | statement                   {   $$ = new vector<astNode*>(); $$->push_back($1);}
              ;

statement  : WHILE '(' condition ')' statement              { $$ = createWhile($3, $5); }
           | IF '(' condition ')' statement %prec IFX       { $$ = createIf($3, $5, NULL); }
           | IF '(' condition ')' statement ELSE statement  { $$ = createIf($3, $5, $7); }
           | declaration        { $$ = $1; }
           | return_statement   { $$ = $1; }
           | print_statement    { $$ = $1; }
           | assignment ';'     { $$ = $1; }
           | expr ';'           { $$ = $1; }
           | block              { $$ = $1; }
           ;

expr : expr '+' term            { $$ = createBExpr($1, $3, add); }
     | expr '-' term            { $$ = createBExpr($1, $3, sub); }
     | term                     { $$ = $1; }
     ;

term   : term '*' factor        { $$ = createBExpr($1, $3, mul); }
       | term '/' factor        { $$ = createBExpr($1, $3, divide); }
       | factor                 { $$ = $1; }
       ;

factor : ID                     {   $$ = createVar($1);  }
         | NUM                  {   $$ = createCnst($1); }
         | READ '(' ')'         {   $$ = createCall("read", NULL); }
         | '(' expr ')'         {   $$ = $2; }
         ;

%%

int yyerror(const char *s){
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
