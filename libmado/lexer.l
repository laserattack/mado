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
    static const char q_prefix[] = "Here: ";
    static const int q_n = sizeof(q_prefix);
    int col, token_start, token_end;

    col = yycolno - yyleng;
    if (col < 1) col = 1;
    token_start = col+q_n-1;
    token_end = token_start + yyleng - 1;

    fprintf(stderr, "%s at line %d, col %d: ", prefix, yylineno, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    if (g_current_query) {
        fprintf(stderr, "%s%s\n", q_prefix, g_current_query);
        for (int i = 1; i <= token_end; i++)
            fprintf(stderr, (i < token_start ? " " : "^"));
        fprintf(stderr, "\n");
    }
}

void yyerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    some_error("Error", fmt, args);
    va_end(args);
}

// return offset and construct base name
static int parse_macro_with_offset(const char *base_name_potential_and_offset, char **base_name) {
    char *clean = malloc(strlen(base_name_potential_and_offset) + 1);
    int j = 0;
    for (int i = 0; base_name_potential_and_offset[i]; i++)
        if (!is_whitespace(base_name_potential_and_offset[i]))
            clean[j++] = base_name_potential_and_offset[i];
    clean[j] = '\0';

    int offset = 0;
    const char *plus = strchr(clean, '+');
    const char *minus = strchr(clean, '-');
    const char *sign = plus ? plus : minus;

    if (sign) {
        size_t base_len = sign - clean;
        *base_name = strndup(clean, base_len);
        offset = atoi(sign + 1);
        if (*sign == '-') offset = -offset;
    } else {
        *base_name = strdup(clean);
    }

    free(clean);
    return offset;
}

// parse macro name without offset, construct base name
void parse_macro_without_offset(const char *base_name_potential, char **base_name) {
    char *clean = malloc(strlen(base_name_potential) + 1);
    int j = 0;
    for (int i = 0; base_name_potential[i]; i++)
        if (!is_whitespace(base_name_potential[i]))
            clean[j++] = base_name_potential[i];
    clean[j] = '\0';
    *base_name = strdup(clean);
    free(clean);
}

static char *resolve_time_macro(const char *base_name_and_offset) {
    static const struct ident_entry base_names[] = {
        {"now",   1},
        {"today", 2},
        {"yesterday", 3},
        {"tomorrow", 4},
        {"week",  5},
        {"month", 6},
        {"year",  7},
        {"never", 8},
    };
    static const int n = sizeof(base_names) / sizeof(base_names[0]);
    static const int resolved_max_len = 16; // sizeof(YYYYMMDDTHHMMSS\0)

    // get offset and base name
    char *base_name_potential = NULL;
    int offset = parse_macro_with_offset(base_name_and_offset, &base_name_potential);
    int base_name = lookup_ident(base_name_potential, base_names, n);
    free(base_name_potential);

    if (!base_name)
        return NULL;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char *resolved = malloc(resolved_max_len);

    switch (base_name) {
    case 1: // now
        {
            time_t t = now + offset * 86400;
            struct tm *ttm = localtime(&t);
            strftime(resolved, resolved_max_len, "%Y%m%dT%H%M%S", ttm);
        }
        break;
    case 2: // today
        {
            time_t t = now + offset * 86400;
            struct tm *ttm = localtime(&t);
            strftime(resolved, resolved_max_len, "%Y%m%d", ttm);
        }
        break;
    case 3: // yesterday
        {
            time_t t = now + (offset - 1) * 86400;
            struct tm *ttm = localtime(&t);
            strftime(resolved, resolved_max_len, "%Y%m%d", ttm);
        }
        break;
    case 4: // tomorrow
        {
            time_t t = now + (offset + 1) * 86400;
            struct tm *ttm = localtime(&t);
            strftime(resolved, resolved_max_len, "%Y%m%d", ttm);
        }
        break;
    case 5: // week
        {
            int days_since_week = (tm->tm_wday + 6) % 7;
            time_t t = now - days_since_week * 86400 + offset * 7 * 86400;
            struct tm *ttm = localtime(&t);
            strftime(resolved, resolved_max_len, "%Y%m%d", ttm);
        }
        break;
    case 6: // month
        {
            int total_months = (tm->tm_year + 1900) * 12 + tm->tm_mon + offset;
            int year = total_months / 12;
            int month = total_months % 12 + 1;
            snprintf(resolved, resolved_max_len, "%04d%02d", year, month);
        }
        break;
    case 7: // year
        {
            int year = tm->tm_year + 1900 + offset;
            snprintf(resolved, resolved_max_len, "%04d", year);
        }
        break;
    case 8: // never
        {
            // timestamp default value: 253370764800 = 99990101T000000
            time_t t = 253370764800 + offset * 86400;
            struct tm *ttm = gmtime(&t);
            strftime(resolved, resolved_max_len, "%Y%m%dT%H%M%S", ttm);
        }
        break;
    }

    return resolved;
}

static char *resolve_number_macro(const char *base_name_without_offset) {
    static const struct ident_entry base_names[] = {
        {"max", 1},
        {"min", 2},
    };
    static const int n = sizeof(base_names) / sizeof(base_names[0]);
    static const int resolved_max_len = 4; // sizeof(999\0)

    // get base name
    char *base_name_potential = NULL;
    parse_macro_without_offset(base_name_without_offset, &base_name_potential);
    int base_name = lookup_ident(base_name_potential, base_names, n);
    free(base_name_potential);

    if (!base_name)
        return NULL;

    int value = 0;
    switch (base_name) {
    case 1: // max
        value = 999;
        break;
    case 2: // min
        value = 0;
        break;
    }

    char *resolved = malloc(resolved_max_len);
    snprintf(resolved, resolved_max_len, "%d", value);
    return resolved;
}

%}

NUMBER             [0-9]{1,3}
/* static char is_whitespace(char c) */
NEWLINE            \n
WHITESPACE         [ \t\r]

/* STRING          \"(          */
/*                    \\["'\\]| */
/*                      [^\"\\] */
/*                   )*         */
/*                 \"|          */
/*                 \'(          */
/*                    \\["'\\]| */
/*                      [^\'\\] */
/*                   )*         */
/*                 \'           */

STRING             \"(\\["'\\]|[^\"\\])*\"|\'(\\["'\\]|[^\'\\])*\'

IDENT              [a-zA-Z_][a-zA-Z0-9_-]*

YEAR               [0-9]{4}
MONTH              (0[1-9]|1[0-2])
DAY                (0[1-9]|[12][0-9]|3[01])
HOUR               ([01][0-9]|2[0-3])
MIN                [0-5][0-9]
SEC                [0-5][0-9]

/* TIMEFORMAT      (                             */
/*                  {YEAR}                       */
/*                  |{YEAR}{MONTH}               */
/*                  |{YEAR}{MONTH}{DAY}          */
/*                     (T                        */
/*                         (                     */
/*                             {HOUR}            */
/*                             |{HOUR}{MIN}      */
/*                             |{HOUR}{MIN}{SEC} */
/*                         )?                    */
/*                     )?                        */
/*                 )                             */

TIMEFORMAT         ({YEAR}|{YEAR}{MONTH}|{YEAR}{MONTH}{DAY}(T({HOUR}|{HOUR}{MIN}|{HOUR}{MIN}{SEC})?)?)

MACRO              @{IDENT}({WHITESPACE}*[+-]{WHITESPACE}*[0-9]{1,6})?

%%

{WHITESPACE}+   { yycolno += yyleng; }
{NEWLINE}+      { yylineno++; yycolno = 1; }

"("             { yycolno += yyleng; return TOKEN_LPAREN; }
")"             { yycolno += yyleng; return TOKEN_RPAREN; }
","             { yycolno += yyleng; return TOKEN_COMMA; }
".."            { yycolno += yyleng; return TOKEN_DOTDOT; }
"["             { yycolno += yyleng; return TOKEN_LBRACKET; }
"]"             { yycolno += yyleng; return TOKEN_RBRACKET; }

">"             { yycolno += yyleng; return TOKEN_GT; }
"<"             { yycolno += yyleng; return TOKEN_LT; }
">="            { yycolno += yyleng; return TOKEN_GE; }
"<="            { yycolno += yyleng; return TOKEN_LE; }
"="             { yycolno += yyleng; return TOKEN_EQ; }
"!="            { yycolno += yyleng; return TOKEN_NE; }

"~~"            { yycolno += yyleng; return TOKEN_FUZZY; }
"!~~"           { yycolno += yyleng; return TOKEN_NFUZZY; }

"^~"            { yycolno += yyleng; return TOKEN_TOKEN_STARTS; }
"^!~"           { yycolno += yyleng; return TOKEN_TOKEN_NSTARTS; }
"~^"            { yycolno += yyleng; return TOKEN_TOKEN_STARTS; }
"!~^"           { yycolno += yyleng; return TOKEN_TOKEN_NSTARTS; }

"$~"            { yycolno += yyleng; return TOKEN_TOKEN_ENDS; }
"$!~"           { yycolno += yyleng; return TOKEN_TOKEN_NENDS; }
"~$"            { yycolno += yyleng; return TOKEN_TOKEN_ENDS; }
"!~$"           { yycolno += yyleng; return TOKEN_TOKEN_NENDS; }

"~"             { yycolno += yyleng; return TOKEN_SUBSTR; }
"!~"            { yycolno += yyleng; return TOKEN_NSUBSTR; }


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
                          static const struct ident_entry keywords[] = {
                              {"deadline",     TOKEN_DEADLINE},
                              {"priority",     TOKEN_PRIORITY},
                              {"status",       TOKEN_STATUS},
                              {"time",         TOKEN_TIME},
                              {"mtime",        TOKEN_MTIME},
                              {"name",         TOKEN_NAME},
                              {"path",         TOKEN_PATH},
                              {"tag",          TOKEN_TAG},
                              {"and",          TOKEN_AND},
                              {"or",           TOKEN_OR},
                              {"xor",          TOKEN_XOR},
                              {"not",          TOKEN_NOT},
                              {"allof",        TOKEN_ALLOF},
                              {"anyof",        TOKEN_ANYOF},
                              {"in",           TOKEN_IN},
                              {"has",          TOKEN_HAS},
                              {"any",          TOKEN_ANY},
                              {"all",          TOKEN_ALL},
                              {"untagged",     TOKEN_UNTAGGED},
                              {"unstatused",   TOKEN_UNSTATUSED},
                              {"unnamed",      TOKEN_UNNAMED},
                              {"unprioritized",TOKEN_UNPRIORITIZED},
                              {"undeadlined",  TOKEN_UNDEADLINED},
                          };
                          static const int n_keywords = sizeof(keywords) / sizeof(keywords[0]);

                          yylval.str = strdup(yytext);

                          int token = lookup_ident(yytext, keywords, n_keywords);
                          if (token > 0) {
                              yycolno += yyleng;
                              return token;
                          }

                          // default string
                          yycolno += yyleng;
                          return TOKEN_STRING;
                      }

{MACRO}               {
                          char *resolved_time_macro = resolve_time_macro(yytext + 1);
                          if (resolved_time_macro) {
                              yylval.str = resolved_time_macro;
                              yycolno += yyleng;
                              return TOKEN_TIMESTAMP;
                          }

                          char *resolved_number_macro = resolve_number_macro(yytext + 1);
                          if (resolved_number_macro) {
                              yylval.num = atoi(resolved_number_macro);
                              free(resolved_number_macro);
                              yycolno += yyleng;
                              return TOKEN_NUMBER;
                          }

                          yylval.str = strdup(yytext);
                          yycolno += yyleng;
                          return TOKEN_UNRESOLVED;
                      }

{STRING}              {
                          char *raw = strdup(yytext + 1);
                          raw[strlen(raw) - 1] = '\0';

                          char *result = malloc(strlen(raw) + 1);
                          int j = 0;
                          for (int i = 0; raw[i]; i++) {
                              if (raw[i] == '\\' && (raw[i+1] == '"' || raw[i+1] == '\\' || raw[i+1] == '\'')) {
                                  result[j++] = raw[i+1];
                                  i++;
                              } else {
                                  result[j++] = raw[i];
                              }
                          }
                          result[j] = '\0';

                          free(raw);
                          yycolno += yyleng;
                          yylval.str = result;
                          return TOKEN_STRING;
                      }

.                     {
                          yylval.str = strdup(yytext);
                          yycolno += yyleng;
                          return TOKEN_UNRESOLVED;
                      }

%%
