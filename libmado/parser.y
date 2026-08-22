%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern int yylex();
extern void yyerror(const char *fmt, ...);

extern AST_Node *ast_root;

// expected string errors

#define YYERROR_KEYWORD_AS_STRING(name) \
    do { \
        yyerror("syntax error, resolved as keyword \"" name "\", quote it to use as a string"); \
        YYERROR; \
    } while (0)

#define YYERROR_NUMBER_AS_STRING(value) \
    do { \
        yyerror("syntax error, number cannot be used as a string value, quote it to use as a string"); \
        YYERROR; \
    } while (0)

#define YYERROR_TIMESTAMP_AS_STRING(value) \
    do { \
        yyerror("syntax error, timestamp cannot be used as a string value, quote it to use as a string"); \
        YYERROR; \
    } while (0)

// expected timestamp errors

#define YYERROR_TIMESTAMP_FORMAT "\nExpected format: 4 digits, 6 digits or (8 digits + optional (T + 0, 2, 4 or 6 digits))"

#define YYERROR_STRING_AS_TIMESTAMP() \
    do { \
        yyerror("syntax error, string cannot be used as a timestamp value " YYERROR_TIMESTAMP_FORMAT); \
        YYERROR; \
    } while (0)

#define YYERROR_NUMBER_AS_TIMESTAMP() \
    do { \
        yyerror("syntax error, number cannot be used as a timestamp value " YYERROR_TIMESTAMP_FORMAT); \
        YYERROR; \
    } while (0)

#define YYERROR_KEYWORD_AS_TIMESTAMP(name) \
    do { \
        yyerror("syntax error, resolved as keyword \"" name "\", expected timestamp value " YYERROR_TIMESTAMP_FORMAT); \
        YYERROR; \
    } while (0)

// expected number errors

#define YYERROR_NUMBER_FORMAT "\nExpected format: 0-999"

#define YYERROR_STRING_AS_NUMBER() \
    do { \
        yyerror("syntax error, string cannot be used as a numeric value " YYERROR_NUMBER_FORMAT); \
        YYERROR; \
    } while (0)

#define YYERROR_TIMESTAMP_AS_NUMBER() \
    do { \
        yyerror("syntax error, timestamp cannot be used as a numeric value " YYERROR_NUMBER_FORMAT); \
        YYERROR; \
    } while (0)

#define YYERROR_KEYWORD_AS_NUMBER(name) \
    do { \
        yyerror("syntax error, resolved as keyword \"" name "\", expected numeric value " YYERROR_NUMBER_FORMAT); \
        YYERROR; \
    } while (0)

// empty list

#define YYERROR_EMPTY_LIST() \
    do { \
        yyerror("syntax error, empty list not allowed"); \
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
%token TOKEN_TILDE
%token TOKEN_NTILDE
%token TOKEN_LPAREN
%token TOKEN_RPAREN
%token TOKEN_COMMA
%token <num> TOKEN_NUMBER
%token <str> TOKEN_STRING
%token <str> TOKEN_TIMESTAMP

%type <num> cmp_op string_field time_field list_modifier number_value
%type <str> string_value time_value

%type <node> expr condition condition_priority condition_string condition_time
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
    | TOKEN_NOT expr                 { $$ = create_unary_op(OP_NOT, $2); }
    | TOKEN_LPAREN expr TOKEN_RPAREN { $$ = $2; }
    ;

condition:
    condition_priority
    | condition_time
    | condition_string
    ;

// number

condition_priority:
    TOKEN_PRIORITY cmp_op number_value {
        $$ = create_comparison(CMP_PRIORITY, $2, $3, NULL);
    }
    | TOKEN_PRIORITY cmp_op list_modifier TOKEN_LPAREN number_list TOKEN_RPAREN {
        $$ = expand_list(CMP_PRIORITY, $2, NULL, $5, $3);
    }
    | TOKEN_PRIORITY cmp_op list_modifier TOKEN_LPAREN TOKEN_RPAREN {
        $$ = NULL; YYERROR_EMPTY_LIST();
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
    TOKEN_NUMBER      { $$ = $1; }
    | TOKEN_STRING    { free($1); $$ = 0; YYERROR_STRING_AS_NUMBER(); }
    | TOKEN_TIMESTAMP { free($1); $$ = 0; YYERROR_TIMESTAMP_AS_NUMBER(); }
    | TOKEN_TIME      { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("time"); }
    | TOKEN_DEADLINE  { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("deadline"); }
    | TOKEN_PRIORITY  { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("priority"); }
    | TOKEN_STATUS    { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("status"); }
    | TOKEN_PATH      { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("path"); }
    | TOKEN_NAME      { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("name"); }
    | TOKEN_TAG       { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("tag"); }
    | TOKEN_MTIME     { $$ = 0; YYERROR_KEYWORD_AS_NUMBER("mtime"); }
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
    | time_field cmp_op list_modifier TOKEN_LPAREN TOKEN_RPAREN {
        $$ = NULL; YYERROR_EMPTY_LIST();
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
    TOKEN_TIMESTAMP   { $$ = $1; }
    | TOKEN_STRING    { free($1); $$ = NULL; YYERROR_STRING_AS_TIMESTAMP(); }
    | TOKEN_NUMBER    { $$ = NULL; YYERROR_NUMBER_AS_TIMESTAMP(); }
    | TOKEN_TIME      { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("time"); }
    | TOKEN_DEADLINE  { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("deadline"); }
    | TOKEN_PRIORITY  { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("priority"); }
    | TOKEN_STATUS    { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("status"); }
    | TOKEN_PATH      { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("path"); }
    | TOKEN_NAME      { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("name"); }
    | TOKEN_TAG       { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("tag"); }
    | TOKEN_MTIME     { $$ = NULL; YYERROR_KEYWORD_AS_TIMESTAMP("mtime"); }
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
    | string_field cmp_op list_modifier TOKEN_LPAREN TOKEN_RPAREN {
        $$ = NULL; YYERROR_EMPTY_LIST();
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
    TOKEN_STRING      { $$ = $1; }
    | TOKEN_TIMESTAMP { free($1); $$ = NULL; YYERROR_TIMESTAMP_AS_STRING(); }
    | TOKEN_NUMBER    { $$ = NULL; YYERROR_NUMBER_AS_STRING(); }
    | TOKEN_TIME      { $$ = NULL; YYERROR_KEYWORD_AS_STRING("time"); }
    | TOKEN_DEADLINE  { $$ = NULL; YYERROR_KEYWORD_AS_STRING("deadline"); }
    | TOKEN_PRIORITY  { $$ = NULL; YYERROR_KEYWORD_AS_STRING("priority"); }
    | TOKEN_STATUS    { $$ = NULL; YYERROR_KEYWORD_AS_STRING("status"); }
    | TOKEN_PATH      { $$ = NULL; YYERROR_KEYWORD_AS_STRING("path"); }
    | TOKEN_NAME      { $$ = NULL; YYERROR_KEYWORD_AS_STRING("name"); }
    | TOKEN_TAG       { $$ = NULL; YYERROR_KEYWORD_AS_STRING("tag"); }
    | TOKEN_MTIME     { $$ = NULL; YYERROR_KEYWORD_AS_STRING("mtime"); }
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
    | TOKEN_TILDE    { $$ = CMP_TILDE; }
    | TOKEN_NTILDE   { $$ = CMP_NTILDE; }
    | TOKEN_GT       { $$ = CMP_GT; }
    | TOKEN_LT       { $$ = CMP_LT; }
    | TOKEN_GE       { $$ = CMP_GE; }
    | TOKEN_LE       { $$ = CMP_LE; }
    ;

list_modifier:
    TOKEN_ALLOF   { $$ = LM_ALLOF; }
    | TOKEN_ANYOF { $$ = LM_ANYOF; }
    ;

%%
