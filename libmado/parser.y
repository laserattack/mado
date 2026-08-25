%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern int yylex();
extern void yyerror(const char *fmt, ...);

extern AST_Node *ast_root;

// expected string errors

#define YYERROR_STRING_EXPECTED "Expected: unquoted [a-zA-Z_][a-zA-Z0-9_-]* or quoted \"...\" or '...'"

#define YYERROR_NUMBER_AS_STRING(value) \
    do { \
        yyerror("syntax error, expected string value, resolved to number \"%d\"\n" \
                YYERROR_STRING_EXPECTED, value); \
        YYERROR; \
    } while (0)

#define YYERROR_TIMESTAMP_AS_STRING(value) \
    do { \
        yyerror("syntax error, expected string value, resolved to timestamp \"%s\"\n" \
                YYERROR_STRING_EXPECTED, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_PUNCTUATION_AS_STRING(value) \
    do { \
        yyerror("syntax error, expected string value, resolved to punctuation \"%s\"\n" \
                YYERROR_STRING_EXPECTED, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_KEYWORD_AS_STRING(name) \
    do { \
        yyerror("syntax error, expected string value, resolved to keyword \"%s\"\n" \
                YYERROR_STRING_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_OPERATOR_AS_STRING(name) \
    do { \
        yyerror("syntax error, expected string value, resolved to operator \"%s\"\n" \
                YYERROR_STRING_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_SPECIAL_AS_STRING(name) \
    do { \
        yyerror("syntax error, expected string value, resolved to special keyword \"%s\"\n" \
                YYERROR_STRING_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

// expected timestamp errors

#define YYERROR_TIMESTAMP_EXPECTED "Expected: YYYY, YYYYMM, YYYYMMDD, YYYYMMDDT, YYYYMMDDTHH, YYYYMMDDTHHMM, YYYYMMDDTHHMMSS"

#define YYERROR_STRING_AS_TIMESTAMP(value) \
    do { \
        yyerror("syntax error, expected timestamp value, resolved to string \"%s\"\n" \
                YYERROR_TIMESTAMP_EXPECTED, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_NUMBER_AS_TIMESTAMP(value) \
    do { \
        yyerror("syntax error, expected timestamp value, resolved to number \"%d\"\n" \
                YYERROR_TIMESTAMP_EXPECTED, value); \
        YYERROR; \
    } while (0)

#define YYERROR_PUNCTUATION_AS_TIMESTAMP(value) \
    do { \
        yyerror("syntax error, expected timestamp value, resolved to punctuation \"%s\"\n" \
                YYERROR_TIMESTAMP_EXPECTED, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_KEYWORD_AS_TIMESTAMP(name) \
    do { \
        yyerror("syntax error, expected timestamp value, resolved to keyword \"%s\"\n" \
                YYERROR_TIMESTAMP_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_OPERATOR_AS_TIMESTAMP(name) \
    do { \
        yyerror("syntax error, expected timestamp value, resolved to operator \"%s\"\n" \
                YYERROR_TIMESTAMP_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_SPECIAL_AS_TIMESTAMP(name) \
    do { \
        yyerror("syntax error, expected timestamp value, resolved to special keyword \"%s\"\n" \
                YYERROR_TIMESTAMP_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

// expected number errors

#define YYERROR_NUMBER_FORMAT "Expected format: 0-999"

#define YYERROR_STRING_AS_NUMBER(value) \
    do { \
        yyerror("syntax error, expected numeric value, resolved to string \"%s\"\n" \
                YYERROR_NUMBER_FORMAT, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_TIMESTAMP_AS_NUMBER(value) \
    do { \
        yyerror("syntax error, expected numeric value, resolved to timestamp \"%s\"\n" \
                YYERROR_NUMBER_FORMAT, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_PUNCTUATION_AS_NUMBER(value) \
    do { \
        yyerror("syntax error, expected numeric value, resolved to punctuation \"%s\"\n" \
                YYERROR_NUMBER_FORMAT, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_KEYWORD_AS_NUMBER(name) \
    do { \
        yyerror("syntax error, expected numeric value, resolved to keyword \"%s\"\n" \
                YYERROR_NUMBER_FORMAT, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_OPERATOR_AS_NUMBER(name) \
    do { \
        yyerror("syntax error, expected numeric value, resolved to operator \"%s\"\n" \
                YYERROR_NUMBER_FORMAT, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_SPECIAL_AS_NUMBER(name) \
    do { \
        yyerror("syntax error, expected numeric value, resolved to special keyword \"%s\"\n" \
                YYERROR_NUMBER_FORMAT, name); \
        free(name); \
        YYERROR; \
    } while (0)

// expected comparison operator errors

#define YYERROR_CMP_EXPECTED "Expected: =, !=, ~, !~, ~~, !~~, >, <, >=, <="

#define YYERROR_STRING_AS_CMP(value) \
    do { \
        yyerror("syntax error, expected comparison operator, resolved to string \"%s\"\n" \
                YYERROR_CMP_EXPECTED, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_TIMESTAMP_AS_CMP(value) \
    do { \
        yyerror("syntax error, expected comparison operator, resolved to timestamp \"%s\"\n" \
                YYERROR_CMP_EXPECTED, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_KEYWORD_AS_CMP(name) \
    do { \
        yyerror("syntax error, expected comparison operator, resolved to keyword \"%s\"\n" \
                YYERROR_CMP_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_OPERATOR_AS_CMP(name) \
    do { \
        yyerror("syntax error, expected comparison operator, resolved to logical operator \"%s\"\n" \
                YYERROR_CMP_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

#define YYERROR_PUNCTUATION_AS_CMP(value) \
    do { \
        yyerror("syntax error, expected comparison operator, resolved to punctuation \"%s\"\n" \
                YYERROR_CMP_EXPECTED, value); \
        free(value); \
        YYERROR; \
    } while (0)

#define YYERROR_SPECIAL_AS_CMP(name) \
    do { \
        yyerror("syntax error, expected comparison operator, resolved to special keyword \"%s\"\n" \
                YYERROR_CMP_EXPECTED, name); \
        free(name); \
        YYERROR; \
    } while (0)

//

#define YYERROR_UNRESOLVED_TOKEN(value) \
    do { \
        yyerror("syntax error, unresolved token \"%s\"", value); \
        free(value); \
        YYERROR; \
    } while (0)

%}

%union {
    int num;
    char *str;
    struct String_List *str_list;
    struct Num_List *num_list;
    struct AST_Node *node;
}

%token TOKEN_AND
%token TOKEN_OR
%token TOKEN_XOR
%token TOKEN_NOT
%token TOKEN_DEADLINE
%token TOKEN_PRIORITY
%token TOKEN_TAG
%token TOKEN_STATUS
%token TOKEN_NAME
%token TOKEN_PATH
%token TOKEN_TIME
%token TOKEN_MTIME
%token TOKEN_ALL
%token TOKEN_ALLOF
%token TOKEN_ANYOF
%token TOKEN_GT
%token TOKEN_LT
%token TOKEN_GE
%token TOKEN_LE
%token TOKEN_EQ
%token TOKEN_NE
%token TOKEN_FUZZY
%token TOKEN_NFUZZY
%token TOKEN_TILDE
%token TOKEN_NTILDE
%token TOKEN_LPAREN
%token TOKEN_RPAREN
%token TOKEN_COMMA

%token <num> TOKEN_NUMBER
%token <str> TOKEN_STRING
%token <str> TOKEN_TIMESTAMP
%token <str> TOKEN_UNRESOLVED // special

%type <num> cmp_op string_field time_field number_field list_modifier number_value
%type <str> string_value time_value keyword_token punctuation_token operator_token special_token
%type <str> logical_operator_token cmp_operator_token
%type <node> expr condition condition_number condition_string condition_time
%type <str_list> string_list time_list
%type <num_list> number_list

%destructor { ast_free($$); } <node>
%destructor { free($$); } <str>
%destructor {
    for (int i = 0; i < $$->count; i++) free($$->items[i]);
    free($$->items);
    free($$);
} <str_list>
%destructor { free($$->items); free($$); } <num_list>

%left TOKEN_OR
%left TOKEN_XOR
%left TOKEN_AND
%right TOKEN_NOT

%start input

%define parse.error verbose

%%

input:
    expr { ast_root = $1; }
    | error {
        if (ast_root) { ast_free(ast_root); ast_root = NULL; }
        YYABORT;
      }
    ;

expr:
    condition                        { $$ = $1; }
    | TOKEN_ALL                      { $$ = create_all(); }
    | expr TOKEN_AND expr            { $$ = create_binary_op(OP_AND, $1, $3); }
    | expr TOKEN_OR expr             { $$ = create_binary_op(OP_OR, $1, $3); }
    | expr TOKEN_XOR expr            { $$ = create_binary_op(OP_XOR, $1, $3); }
    | TOKEN_NOT expr                 { $$ = create_unary_op(OP_NOT, $2); }
    | TOKEN_LPAREN expr TOKEN_RPAREN { $$ = $2; }
    ;

condition:
    condition_number
    | condition_time
    | condition_string
    ;

// number

condition_number:
    number_field cmp_op number_value {
        $$ = create_comparison(CMP_PRIORITY, $2, $3, NULL);
    }
    | number_field cmp_op list_modifier TOKEN_LPAREN number_list TOKEN_RPAREN {
        $$ = expand_list(CMP_PRIORITY, $2, NULL, $5, $3);
    }
    ;

number_list:
    number_value { $$ = create_number_list($1); }
    | number_list TOKEN_COMMA number_value {
        append_number($1, $3);
        $$ = $1;
    }
    ;

number_value:
    TOKEN_NUMBER        { $$ = $1; }
    // unexpected
    | TOKEN_STRING      { $$ = 0; YYERROR_STRING_AS_NUMBER($1); }
    | TOKEN_TIMESTAMP   { $$ = 0; YYERROR_TIMESTAMP_AS_NUMBER($1); }
    | TOKEN_UNRESOLVED  { $$ = 0; YYERROR_UNRESOLVED_TOKEN($1); }
    | keyword_token     { $$ = 0; YYERROR_KEYWORD_AS_NUMBER($1); }
    | operator_token    { $$ = 0; YYERROR_OPERATOR_AS_NUMBER($1); }
    | punctuation_token { $$ = 0; YYERROR_PUNCTUATION_AS_NUMBER($1); }
    | special_token     { $$ = 0; YYERROR_SPECIAL_AS_NUMBER($1); }
    ;

number_field:
    TOKEN_PRIORITY { $$ = CMP_PRIORITY; }
    ;

//

// time

condition_time:
    time_field cmp_op time_value {
        $$ = create_comparison($1, $2, 0, $3);
    }
    | time_field cmp_op list_modifier TOKEN_LPAREN time_list TOKEN_RPAREN {
        $$ = expand_list($1, $2, $5, NULL, $3);
    }
    ;

time_list:
    time_value { $$ = create_string_list($1); }
    | time_list TOKEN_COMMA time_value {
        append_string($1, $3);
        $$ = $1;
    }
    ;

time_value:
    TOKEN_TIMESTAMP     { $$ = $1; }
    // unexpected
    | TOKEN_STRING      { $$ = NULL; YYERROR_STRING_AS_TIMESTAMP($1); }
    | TOKEN_NUMBER      { $$ = NULL; YYERROR_NUMBER_AS_TIMESTAMP($1); }
    | TOKEN_UNRESOLVED  { $$ = NULL; YYERROR_UNRESOLVED_TOKEN($1); }
    | keyword_token     { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP($1); }
    | operator_token    { $$ = NULL; YYERROR_OPERATOR_AS_TIMESTAMP($1); }
    | punctuation_token { $$ = NULL; YYERROR_PUNCTUATION_AS_TIMESTAMP($1); }
    | special_token     { $$ = NULL; YYERROR_SPECIAL_AS_TIMESTAMP($1); }
    ;

time_field:
    TOKEN_TIME       { $$ = CMP_TIME; }
    | TOKEN_DEADLINE { $$ = CMP_DEADLINE; }
    | TOKEN_MTIME    { $$ = CMP_MTIME; }
    ;

//

// string

condition_string:
    string_field cmp_op string_value {
        $$ = create_comparison($1, $2, 0, $3);
    }
    | string_field cmp_op list_modifier TOKEN_LPAREN string_list TOKEN_RPAREN {
        $$ = expand_list($1, $2, $5, NULL, $3);
    }
    ;

string_list:
    string_value { $$ = create_string_list($1); }
    | string_list TOKEN_COMMA string_value {
        append_string($1, $3);
        $$ = $1;
    }
    ;

string_value:
    // unexpected
    TOKEN_STRING        { $$ = $1; }
    | TOKEN_TIMESTAMP   { $$ = NULL; YYERROR_TIMESTAMP_AS_STRING($1); }
    | TOKEN_NUMBER      { $$ = NULL; YYERROR_NUMBER_AS_STRING($1); }
    | TOKEN_UNRESOLVED  { $$ = NULL; YYERROR_UNRESOLVED_TOKEN($1); }
    | keyword_token     { $$ = NULL; YYERROR_KEYWORD_AS_STRING($1); }
    | operator_token    { $$ = NULL; YYERROR_OPERATOR_AS_STRING($1); }
    | punctuation_token { $$ = NULL; YYERROR_PUNCTUATION_AS_STRING($1); }
    | special_token     { $$ = NULL; YYERROR_SPECIAL_AS_STRING($1); }
    ;

string_field:
    TOKEN_TAG      { $$ = CMP_TAG; }
    | TOKEN_STATUS { $$ = CMP_STATUS; }
    | TOKEN_PATH   { $$ = CMP_PATH; }
    | TOKEN_NAME   { $$ = CMP_NAME; }
    ;

//

cmp_op:
    TOKEN_EQ         { $$ = CMP_EQ; }
    | TOKEN_NE       { $$ = CMP_NE; }
    | TOKEN_FUZZY    { $$ = CMP_FUZZY; }
    | TOKEN_NFUZZY   { $$ = CMP_NFUZZY; }
    | TOKEN_TILDE    { $$ = CMP_TILDE; }
    | TOKEN_NTILDE   { $$ = CMP_NTILDE; }
    | TOKEN_GT       { $$ = CMP_GT; }
    | TOKEN_LT       { $$ = CMP_LT; }
    | TOKEN_GE       { $$ = CMP_GE; }
    | TOKEN_LE       { $$ = CMP_LE; }
    // unexpected
    | TOKEN_STRING           { $$ = 0; YYERROR_STRING_AS_CMP($1); }
    | TOKEN_TIMESTAMP        { $$ = 0; YYERROR_TIMESTAMP_AS_CMP($1); }
    | TOKEN_UNRESOLVED       { $$ = 0; YYERROR_UNRESOLVED_TOKEN($1); }
    | keyword_token          { $$ = 0; YYERROR_KEYWORD_AS_CMP($1); }
    | logical_operator_token { $$ = 0; YYERROR_OPERATOR_AS_CMP($1); }
    | punctuation_token      { $$ = 0; YYERROR_PUNCTUATION_AS_CMP($1); }
    | special_token          { $$ = 0; YYERROR_SPECIAL_AS_CMP($1); }
    ;

list_modifier:
    TOKEN_ALLOF   { $$ = LM_ALLOF; }
    | TOKEN_ANYOF { $$ = LM_ANYOF; }
    ;

// tokens (for errors)

keyword_token:
    TOKEN_TIME       { $$ = strdup("time"); }
    | TOKEN_DEADLINE { $$ = strdup("deadline"); }
    | TOKEN_PRIORITY { $$ = strdup("priority"); }
    | TOKEN_STATUS   { $$ = strdup("status"); }
    | TOKEN_PATH     { $$ = strdup("path"); }
    | TOKEN_NAME     { $$ = strdup("name"); }
    | TOKEN_TAG      { $$ = strdup("tag"); }
    | TOKEN_MTIME    { $$ = strdup("mtime"); }
    | TOKEN_ALLOF    { $$ = strdup("allof"); }
    | TOKEN_ANYOF    { $$ = strdup("anyof"); }
    ;

special_token:
    TOKEN_ALL        { $$ = strdup("all"); }
    ;

operator_token:
    logical_operator_token
    | cmp_operator_token
    ;

logical_operator_token:
    TOKEN_AND       { $$ = strdup("and"); }
    | TOKEN_OR      { $$ = strdup("or"); }
    | TOKEN_XOR     { $$ = strdup("xor"); }
    | TOKEN_NOT     { $$ = strdup("not"); }
    ;

cmp_operator_token:
    TOKEN_GT        { $$ = strdup(">"); }
    | TOKEN_LT      { $$ = strdup("<"); }
    | TOKEN_GE      { $$ = strdup(">="); }
    | TOKEN_LE      { $$ = strdup("<="); }
    | TOKEN_EQ      { $$ = strdup("="); }
    | TOKEN_NE      { $$ = strdup("!="); }
    | TOKEN_FUZZY   { $$ = strdup("~~"); }
    | TOKEN_NFUZZY  { $$ = strdup("!~~"); }
    | TOKEN_TILDE   { $$ = strdup("~"); }
    | TOKEN_NTILDE  { $$ = strdup("!~"); }
    ;

punctuation_token:
    TOKEN_LPAREN    { $$ = strdup("("); }
    | TOKEN_RPAREN  { $$ = strdup(")"); }
    | TOKEN_COMMA   { $$ = strdup(","); }
    ;


%%
