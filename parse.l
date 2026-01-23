// trying to tokenize: symbols(braces, assignment) operators, numbers. no recursive definitions

%{
	#include <stdio.h>
	#include "y.tab.h"
%}

%%
[a-zA-Z]+	{ return NAME;}
[0-9]+		{ return NUM;}
'while'     {return WHILE;}
'if'        {return IF;}
'else'      {return ELSE;}
'print'     {return PRINT;}
'int'       {return INT;}
'extern'    {return EXTERN;}
'void'      {return VOID;}
[ \t\n+]







%%

int yywrap(){
	return 1;
}