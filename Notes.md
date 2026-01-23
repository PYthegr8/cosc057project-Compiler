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