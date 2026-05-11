#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// tokens

#define TOKEN_AND       256
#define TOKEN_OR        257
#define TOKEN_NOT       258
#define TOKEN_PRIORITY  259
#define TOKEN_TAG       260
#define TOKEN_GT        261
#define TOKEN_LT        262
#define TOKEN_GE        263
#define TOKEN_LE        264
#define TOKEN_EQ        265
#define TOKEN_NUMBER    266
#define TOKEN_IDENT     267
#define TOKEN_STRING    268
#define TOKEN_LPAREN    269
#define TOKEN_RPAREN    270

// types

// type for values associated with tokens (e.g., number value, string content)
union YYSTYPE {
    int  num;
    char *str;
};

// vars

extern union YYSTYPE yylval; // in lexer.l
extern FILE *yyin; // in lex.yy.c (generated)

// funcs

extern int yylex(void); // in lex.yy.c (generated)
extern int yywrap(void); // in lexer.l
extern void yyerror(const char *fmt, ...); // in lexer.l

#endif
