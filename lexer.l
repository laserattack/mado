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

NUMBER             [0-9]{1,3}
WHITESPACE         [ \t\r]
PRIORITY           priority|PRIORITY
STATUS             status|STATUS
TIME               time|TIME
NAME               name|NAME
TAG                tag|TAG
AND                and|AND
OR                 or|OR
NOT                not|NOT
ALL                all|ALL

ALLOF              allof
ANYOF              anyof

TIMEFORMAT         ([0-9]{4}|[0-9]{6}|[0-9]{8})(T([0-9]{2}|[0-9]{4}|[0-9]{6})?)?
QUOTED_TIMEFORMAT  \"{TIMEFORMAT}\"|\'{TIMEFORMAT}\'
IDENT              [a-zA-Z_][a-zA-Z0-9_-]*
STRING             \"[^\"]*\"|\'[^\']*\'
NEWLINE            \n

%%

{WHITESPACE}+   { /* ignore */ }
{NEWLINE}+      { /* ignore */ }

{PRIORITY}      { return TOKEN_PRIORITY; }
{STATUS}        { return TOKEN_STATUS; }
{TIME}          { return TOKEN_TIME; }
{NAME}          { return TOKEN_NAME; }
{TAG}           { return TOKEN_TAG; }
{AND}           { return TOKEN_AND; }
{OR}            { return TOKEN_OR; }
{NOT}           { return TOKEN_NOT; }
{ALL}           { return TOKEN_ALL; }

{ALLOF}         { return TOKEN_ALLOF; }
{ANYOF}         { return TOKEN_ANYOF; }

","             { return TOKEN_COMMA; }
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


{NUMBER}              {
                          yylval.num = atoi(yytext);
                          return TOKEN_NUMBER;
                      }

{TIMEFORMAT}          {
                          yylval.str = strdup(yytext);
                          return TOKEN_TIME_VALUE;
                      }

{QUOTED_TIMEFORMAT}   {
                         yytext[strlen(yytext)-1] = '\0';
                         yylval.str = strdup(yytext+1);
                         return TOKEN_TIME_VALUE;
                      }

{IDENT}               {
                          yylval.str = strdup(yytext);
                          return TOKEN_IDENT;
                      }

{STRING}              {
                         yytext[strlen(yytext)-1] = '\0';
                         yylval.str = strdup(yytext+1);
                         return TOKEN_STRING;
                      }

.                     {
                          yyerror("unexpected char: '%s'", yytext);
                      }

%%
