%{

#include "lexer.h"

union YYSTYPE yylval;

int yywrap(void) {
    return 1;
}

void yyerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Tokenize error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

%}

DIGIT       [0-9]
WHITESPACE  [ \t\r]
PRIORITY    priority
TAG         tag
AND         and|AND
OR          or|OR
NOT         not|NOT
IDENT       [a-zA-Z_][a-zA-Z0-9_-]*
STRING      \"[^\"]*\"
NEWLINE     \n

%%

{WHITESPACE}+   { /* ignore */ }
{NEWLINE}+      { /* ignore */ }

{PRIORITY}      { return TOKEN_PRIORITY; }
{TAG}           { return TOKEN_TAG; }
{AND}           { return TOKEN_AND; }
{OR}            { return TOKEN_OR; }
{NOT}           { return TOKEN_NOT; }

">"             { return TOKEN_GT; }
"<"             { return TOKEN_LT; }
">="            { return TOKEN_GE; }
"<="            { return TOKEN_LE; }
"="             { return TOKEN_EQ; }

"("             { return TOKEN_LPAREN; }
")"             { return TOKEN_RPAREN; }

{DIGIT}+        {
                    yylval.num = atoi(yytext);
                    return TOKEN_NUMBER;
                }

{IDENT}         {
                    yylval.str = strdup(yytext);
                    return TOKEN_IDENT;
                }

{STRING}        {
                    yytext[strlen(yytext)-1] = '\0';
                    yylval.str = strdup(yytext+1);
                    return TOKEN_STRING;
                }

.               {
                    yyerror("unexpected char: '%s'\n", yytext);
                    return -1;
                }

%%
