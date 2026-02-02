# Compiler Project Notes

## 1. Lexical Analysis (`.l` file)

The **Lex** file identifies "tokens" (words) in the source code using **Regular Expressions**.

* **Rule Structure:** `[regex] { action }`
* **Returns:** When a regex matches, the action returns a **Terminal Symbol** (e.g., `return ID;`).
* **Terminal Symbols:** These are defined in the Yacc file and used to build the grammar.

### Handling Data with `yytext`

`yytext` is a pointer to the actual string matched by the regex. Because `yytext` is temporary, you must copy or convert it:

* **For Integers:** `yylval.ival = atoi(yytext);`
* **For Strings:** `yylval.sval = strdup(yytext);`

---

## 2. The Yacc Connection (`.y` file)

**Yacc** takes the terminal symbols from Lex to check the program's structure.

* **`y.tab.h`:** Yacc automatically generates this header file. It lists all terminal symbols so the Lex file can recognize them.
* **`%union`:** Used in Yacc to define the different data types `yylval` can hold (e.g., `int`, `char*`, `float`).

### Positional Parameters

In Yacc actions, you access the values of the tokens using the `$` syntax:

* `$1`, `$2`, `$3` ... represent the values of the symbols in the rule.
* `$$` represents the value of the ent ire expression being evaluated.

---

## 3. Grammar & Expressions

Grammar rules are built using the terminals provided by Lex.

* **Binary Expressions:** Usually follow the pattern `expression OP expression`.
* **Assignments:** Can be a single term or a complex expression.
* *Example:* `assignment : ID '=' expression`

## Coding Notes
1. An expression is terms added or subtracted 
2. A term is factors multiplied or divided 
3. A factor is an identifier or a number
4. A block is just a group of statements(statement list) wrapped in braces
5. A shift/reduce conflict means “the grammar lets me finish now or keep going.”
   %prec and %nonassoc are how we tell the parser which choice we intend. so the order matters (later = higher precedence). I want ELSE higher than IFX, so put:
%nonassoc IFX
%nonassoc ELSE
6. %union defines all possible semantic value types 
7. %token / %type tell yacc which type each symbol uses 
8. $n and $$ are typed views into that union . AST construction only works because of this typing

## Semantic Analysis Notes
my current plan is to use a stack to keep track of the scope of variables. i passed the root node of the AST to the semantic analyzer and it will traverse the tree and check for errors. 
i provided the root of the ast to the funciton in the yacc file
I'm thinking my symbol table should be a hashed structure , maybe a map or a set. I don't think i need a set since I'm not storing values so i can use a set
My stack will be a vector of those symbol tables sets. 
1. a scope is created for the function itself and for every block surrounded by {}. when i see a declaration, i only look at the current scope. if the variable name is already there, that’s an error because it’s a duplicate declaration in the same scope. otherwise i add it to the current scope. when i see a variable being used, i search from the top of the scope stack downward. if the variable exists in any scope, it’s valid. if it doesn’t exist anywhere, then it’s an undeclared variable error.
2. i want one general traversal function that looks at the type of each node and decides what to do next. for statements specifically, i want a separate “check statement” step. if the statement is a block, i push a new scope, process the statements inside in order, and then pop the scope when the block ends. if it’s a declaration, i treat it as introducing a new variable into the current scope. if it’s an assignment, i check both the left and right sides for variable usage. for conditionals and loops, i check the condition first and then check the body, which may or may not be a block.
3. when i start analyzing a function, i create a new scope and insert the parameter name before walking the function body. that way, the parameter behaves like a normal variable that’s visible everywhere inside the function.