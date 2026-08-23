%{

#include <stdarg.h>
#include <strings.h>
#include <time.h>

#define MADO_UTILS_IMPL
#include "utils.h"

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

static char *resolve_time_macro(const char *varname) {
    static const struct keyword_entry time_vars[] = {
        {"now",   1},
        {"today", 2},
        {"yesterday", 3},
        {"tomorrow", 4},
        {"week",  5},
        {"month", 6},
        {"year",  7},
        {"never", 8},
    };
    static const int n = sizeof(time_vars) / sizeof(time_vars[0]);

    char clean[64];
    int j = 0;
    for (int i = 0; varname[i]; i++)
        if (!is_whitespace(varname[i]))
            clean[j++] = varname[i];
    clean[j] = '\0';

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char buf[64];
    char base[32];
    int offset = 0;

    const char *plus = strchr(clean, '+');
    const char *minus = strchr(clean, '-');
    const char *sign = plus ? plus : minus;

    if (sign) {
        size_t base_len = sign - clean;
        memcpy(base, clean, base_len);
        base[base_len] = '\0';
        offset = atoi(sign + 1);
        if (*sign == '-') offset = -offset;
    } else {
        strcpy(base, clean);
    }

    int var_type = lookup_keyword(base, time_vars, n);
    if (!var_type)
        return NULL;

    switch (var_type) {
    case 1: // now
        {
            time_t t = now + offset * 86400;
            struct tm *ttm = localtime(&t);
            strftime(buf, sizeof(buf), "%Y%m%dT%H%M%S", ttm);
        }
        break;
    case 2: // today
        {
            time_t t = now + offset * 86400;
            struct tm *ttm = localtime(&t);
            strftime(buf, sizeof(buf), "%Y%m%d", ttm);
        }
        break;
    case 3: // yesterday
        {
            time_t t = now + (offset - 1) * 86400;
            struct tm *ttm = localtime(&t);
            strftime(buf, sizeof(buf), "%Y%m%d", ttm);
        }
        break;
    case 4: // tomorrow
        {
            time_t t = now + (offset + 1) * 86400;
            struct tm *ttm = localtime(&t);
            strftime(buf, sizeof(buf), "%Y%m%d", ttm);
        }
        break;
    case 5: // week
        {
            int days_since_monday = (tm->tm_wday + 6) % 7;
            time_t t = now - days_since_monday * 86400 + offset * 7 * 86400;
            struct tm *ttm = localtime(&t);
            strftime(buf, sizeof(buf), "%Y%m%d", ttm);
        }
        break;
    case 6: // month
        {
            int total_months = (tm->tm_year + 1900) * 12 + tm->tm_mon + offset;
            int year = total_months / 12;
            int month = total_months % 12 + 1;
            snprintf(buf, sizeof(buf), "%04d%02d", year, month);
        }
        break;
    case 7: // year
        {
            int year = tm->tm_year + 1900 + offset;
            snprintf(buf, sizeof(buf), "%04d", year);
        }
        break;
    case 8: // never (timestamp default value)
        {
            // 253370764800 = 99990101T000000 (timestamp default value)
            time_t t = 253370764800 + offset * 86400;
            struct tm *ttm = gmtime(&t); // no need timezone here
            strftime(buf, sizeof(buf), "%Y%m%dT%H%M%S", ttm);
        }
        break;
    }

    return strdup(buf);
}

%}

NUMBER             [0-9]{1,3}
/* static char is_whitespace(char c) */
NEWLINE            \n
WHITESPACE         [ \t\r]
STRING             \"[^\"]*\"|\'[^\']*\'
IDENT              [a-zA-Z_][a-zA-Z0-9_-]*

YEAR               [0-9]{4}
MONTH              (0[1-9]|1[0-2])
DAY                (0[1-9]|[12][0-9]|3[01])
HOUR               ([01][0-9]|2[0-3])
MIN                [0-5][0-9]
SEC                [0-5][0-9]

/* TIMEFORMAT      ( */
/*                  {YEAR} */
/*                  |{YEAR}{MONTH} */
/*                  |{YEAR}{MONTH}{DAY} */
/*                     (T */
/*                         ( */
/*                             {HOUR} */
/*                             |{HOUR}{MIN} */
/*                             |{HOUR}{MIN}{SEC} */
/*                         )? */
/*                     )? */
/*                 ) */

TIMEFORMAT         ({YEAR}|{YEAR}{MONTH}|{YEAR}{MONTH}{DAY}(T({HOUR}|{HOUR}{MIN}|{HOUR}{MIN}{SEC})?)?)

/* macros */
TIME_MACRO         @{IDENT}({WHITESPACE}*[+-]{WHITESPACE}*[0-9]{1,6})?

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
"~~"            { yycolno += yyleng; return TOKEN_FUZZY; }
"!~~"           { yycolno += yyleng; return TOKEN_NFUZZY; }
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

{IDENT}               {
                          static const struct keyword_entry keywords[] = {
                              {"deadline", TOKEN_DEADLINE},
                              {"priority", TOKEN_PRIORITY},
                              {"status",   TOKEN_STATUS},
                              {"time",     TOKEN_TIME},
                              {"mtime",    TOKEN_MTIME},
                              {"name",     TOKEN_NAME},
                              {"path",     TOKEN_PATH},
                              {"tag",      TOKEN_TAG},
                              {"and",      TOKEN_AND},
                              {"or",       TOKEN_OR},
                              {"not",      TOKEN_NOT},
                              {"all",      TOKEN_ALL},
                              {"allof",    TOKEN_ALLOF},
                              {"anyof",    TOKEN_ANYOF},
                          };
                          static const int n_keywords = sizeof(keywords) / sizeof(keywords[0]);

                          int token = lookup_keyword(yytext, keywords, n_keywords);
                          if (token > 0) {
                              yycolno += yyleng;
                              return token;
                          }

                          // default string
                          yylval.str = strdup(yytext);
                          yycolno += yyleng;
                          return TOKEN_STRING;
                      }

{TIME_MACRO}          {
                          char *resolved = resolve_time_macro(yytext + 1);

                          if (resolved) {
                              yylval.str = resolved;
                              yycolno += yyleng;
                              return TOKEN_TIMESTAMP;
                          } else {
                              yylval.str = strdup(yytext);
                              yycolno += yyleng;
                              return TOKEN_STRING;
                          }
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
