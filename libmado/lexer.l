%{

#include <stdarg.h>
#include <strings.h>

#include "parser.tab.h"

static const char *g_current_query = NULL;
int yycolno = 1;

void set_query_string(const char *query) {
    g_current_query = query;
}

int yywrap() {
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
TIMEFORMAT         ([0-9]{4}|[0-9]{6}|[0-9]{8}(T([0-9]{2}|[0-9]{4}|[0-9]{6})?)?)
QUOTED_TIMEFORMAT  \"{TIMEFORMAT}\"|\'{TIMEFORMAT}\'
STRING             \"[^\"]*\"|\'[^\']*\'
IDENT              [a-zA-Z_][a-zA-Z0-9_-]*
NEWLINE            \n

%%

{WHITESPACE}+   { yycolno += yyleng; }
{NEWLINE}+      { yylineno++; yycolno = 1; }

","             { yycolno += yyleng; return TOKEN_COMMA; }
">"             { yycolno += yyleng; return TOKEN_GT; }
"<"             { yycolno += yyleng; return TOKEN_LT; }
">="            { yycolno += yyleng; return TOKEN_GE; }
"<="            { yycolno += yyleng; return TOKEN_LE; }
"="             { yycolno += yyleng; return TOKEN_EQ; }
"!="            { yycolno += yyleng; return TOKEN_NE; }
"~"             { yycolno += yyleng; return TOKEN_TILDE; }
"!~"            { yycolno += yyleng; return TOKEN_NTILDE; }
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
                          return TOKEN_TIMESTAMP;
                      }

{QUOTED_TIMEFORMAT}   {
                          yytext[strlen(yytext)-1] = '\0';
                          yylval.str = strdup(yytext+1);
                          yycolno += yyleng;
                          return TOKEN_TIMESTAMP;
                      }

{IDENT}               {
                          // trying to guess the token
                          static const struct {
                              const char *word;
                              int token;
                          } keywords[] = {
                              {"deadline", TOKEN_DEADLINE},
                              {"priority", TOKEN_PRIORITY},
                              {"status",   TOKEN_STATUS},
                              {"time",     TOKEN_TIME},
                              {"name",     TOKEN_NAME},
                              {"tag",      TOKEN_TAG},
                              {"and",      TOKEN_AND},
                              {"or",       TOKEN_OR},
                              {"not",      TOKEN_NOT},
                              {"all",      TOKEN_ALL},
                              {"allof",    TOKEN_ALLOF},
                              {"anyof",    TOKEN_ANYOF},
                          };
                          static const int n_keywords =
                              sizeof(keywords) / sizeof(keywords[0]);

                          int matches_count = 0, matched_token = 0;

                          for (int i = 0; i < n_keywords; i++) {
                              int cur_word_len = (int)strlen(keywords[i].word);
                              if (yyleng <= cur_word_len &&
                                  strncasecmp(yytext, keywords[i].word, yyleng) == 0) {
                                  matched_token = keywords[i].token;
                                  if (yyleng == cur_word_len) {
                                      matches_count = 1;
                                      break;
                                  }
                                  matches_count++;
                              }
                          }

                          if (matches_count == 1) {
                              yycolno += yyleng;
                              return matched_token;
                          }

                          // couldn't guess the token => default string
                          yylval.str = strdup(yytext);
                          yycolno += yyleng;
                          return TOKEN_STRING;
                      }

{STRING}              {
                          yytext[strlen(yytext)-1] = '\0';
                          yylval.str = strdup(yytext+1);
                          yycolno += yyleng;
                          return TOKEN_STRING;
                      }

.                     {
                          yycolno += yyleng;
                          lexerror("unexpected char: '%s', trying to skip...", yytext);
                      }

%%
