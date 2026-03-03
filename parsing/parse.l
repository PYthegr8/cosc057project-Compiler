%{
	#include <stdio.h>
	#include <stdlib.h>
    #include <string.h>
    #include "ast.h"
    #include "parsing.tab.h"
%}

%%
"while"                 {return WHILE;}
"if"                    {return IF;}
"else"                  {return ELSE;}
"print"                 {return PRINT;}
"int"                   {return INT;}
"extern"                {return EXTERN;}
"void"                  {return VOID;}
"return"                {return RETURN;}
"read"                  {return READ;}
[a-zA-Z][a-zA-Z0-9]*	{yylval.sval = strdup(yytext); return ID;}
[0-9]+		            {yylval.ival = atoi(yytext); return NUM;}

[ \t\n]+               ;

"<="                    { return LE; }
">="                    { return GE; }
"=="                    { return EQ; }
"!="                    { return NE; }

.                       { return yytext[0]; }


%%

int yywrap(){
	return 1;
}