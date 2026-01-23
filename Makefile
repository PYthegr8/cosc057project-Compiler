parser: parsing.y parse.l
	bison -d parsing.y
	flex parse.l
	gcc -Wall -Wextra -g -o parser parsing.tab.c lex.yy.c -lfl

clean:
	rm -f parser parsing.tab.c parsing.tab.h lex.yy.c
