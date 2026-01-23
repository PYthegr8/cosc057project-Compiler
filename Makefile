parser: parsing.y parse.l
	bison -d parsing.y
	flex parse.l
	g++ -Wall -Wextra -g -o parser parsing.tab.c lex.yy.c ast.c -lfl

clean:
	rm -f parser parsing.tab.c parsing.tab.h lex.yy.c
