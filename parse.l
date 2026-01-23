%{
	#include <stdio.h>
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
[a-zA-Z][a-zA-Z0-9]*	{return ID;}
[0-9]+		            {return NUM;}

[ \t\n]+               ;
.   { return yytext[0]; }








%%

int yywrap(){
	return 1;
}