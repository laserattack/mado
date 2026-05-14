%{

#include <stdarg.h>

#include "parser.tab.h"

int yywrap(void) {
    return 1;
}

void yyerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

%}

DIGIT       [0-9]
WHITESPACE  [ \t\r]
PRIORITY    priority|PRIORITY
STATUS      status|STATUS
NAME        name|NAME
TAG         tag|TAG
AND         and|AND
OR          or|OR
NOT         not|NOT
ALL         all|ALL
IDENT       [a-zA-Z_][a-zA-Z0-9_-]*
STRING      \"[^\"]*\"
NEWLINE     \n

%%

{WHITESPACE}+   { /* ignore */ }
{NEWLINE}+      { /* ignore */ }

{PRIORITY}      { return TOKEN_PRIORITY; }
{STATUS}        { return TOKEN_STATUS; }
{NAME}          { return TOKEN_NAME; }
{TAG}           { return TOKEN_TAG; }
{AND}           { return TOKEN_AND; }
{OR}            { return TOKEN_OR; }
{NOT}           { return TOKEN_NOT; }
{ALL}           { return TOKEN_ALL; }

">"             { return TOKEN_GT; }
"<"             { return TOKEN_LT; }
">="            { return TOKEN_GE; }
"<="            { return TOKEN_LE; }
"="             { return TOKEN_EQ; }
"!="            { return TOKEN_NE; }
"~"             { return TOKEN_TILDE; }
"!~"            { return TOKEN_NE_TILDE; }

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
                    yyerror("unexpected char: '%s'", yytext);
                }

%%
