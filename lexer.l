%{

#include <stdarg.h>

#include "parser.tab.h"

static const char *g_current_query = NULL;
int yycolno = 1;

void set_query_string(const char *query) {
    g_current_query = query;
}

int yywrap(void) {
    return 1;
}

static void some_error(const char *prefix, const char *fmt, va_list args) {
    int col = yycolno - yyleng;
    if (col < 1) col = 1;
    fprintf(stderr, "%s at line %d, column %d: ", prefix, yylineno, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    if (g_current_query) {
        fprintf(stderr, "%s\n", g_current_query);
        for (int i = 1; i < col; i++)
            fprintf(stderr, " ");
        fprintf(stderr, "^\n");
    }
}

void yyerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    some_error("Error", fmt, args);
    va_end(args);
}

void lexerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    some_error("Warning", fmt, args);
    va_end(args);
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

{WHITESPACE}+   { yycolno += yyleng; }
{NEWLINE}+      { yylineno++; yycolno = 1; }

{PRIORITY}      { yycolno += yyleng; return TOKEN_PRIORITY; }
{STATUS}        { yycolno += yyleng; return TOKEN_STATUS; }
{TIME}          { yycolno += yyleng; return TOKEN_TIME; }
{NAME}          { yycolno += yyleng; return TOKEN_NAME; }
{TAG}           { yycolno += yyleng; return TOKEN_TAG; }
{AND}           { yycolno += yyleng; return TOKEN_AND; }
{OR}            { yycolno += yyleng; return TOKEN_OR; }
{NOT}           { yycolno += yyleng; return TOKEN_NOT; }
{ALL}           { yycolno += yyleng; return TOKEN_ALL; }

{ALLOF}         { yycolno += yyleng; return TOKEN_ALLOF; }
{ANYOF}         { yycolno += yyleng; return TOKEN_ANYOF; }

","             { yycolno += yyleng; return TOKEN_COMMA; }
">"             { yycolno += yyleng; return TOKEN_GT; }
"<"             { yycolno += yyleng; return TOKEN_LT; }
">="            { yycolno += yyleng; return TOKEN_GE; }
"<="            { yycolno += yyleng; return TOKEN_LE; }
"="             { yycolno += yyleng; return TOKEN_EQ; }
"!="            { yycolno += yyleng; return TOKEN_NE; }
"~"             { yycolno += yyleng; return TOKEN_TILDE; }
"!~"            { yycolno += yyleng; return TOKEN_NE_TILDE; }
"("             { yycolno += yyleng; return TOKEN_LPAREN; }
")"             { yycolno += yyleng; return TOKEN_RPAREN; }


{NUMBER}              {
                          yylval.num = atoi(yytext);
                          yycolno += yyleng;
                          return TOKEN_NUMBER;
                      }

{TIMEFORMAT}          {
                          yylval.str = strdup(yytext);
                          yycolno += yyleng;
                          return TOKEN_TIME_VALUE;
                      }

{QUOTED_TIMEFORMAT}   {
                         yytext[strlen(yytext)-1] = '\0';
                         yylval.str = strdup(yytext+1);
                         yycolno += yyleng;
                         return TOKEN_TIME_VALUE;
                      }

{IDENT}               {
                          yylval.str = strdup(yytext);
                          yycolno += yyleng;
                          return TOKEN_IDENT;
                      }

{STRING}              {
                         yytext[strlen(yytext)-1] = '\0';
                         yylval.str = strdup(yytext+1);
                         yycolno += yyleng;
                         return TOKEN_STRING;
                      }

.                     {
                          yycolno += yyleng;
                          lexerror("unexpected char: '%s'. Trying to skip...", yytext);
                      }

%%
