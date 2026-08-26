%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern int yylex();
extern void yyerror(const char *fmt, ...);

extern AST_Node *ast_root;

%}

%union {
    int num;
    char *str;
    struct String_List *str_list;
    struct Num_List *num_list;
    struct AST_Node *node;
}

%token <str> TOKEN_DEADLINE "DEADLINE"
%token <str> TOKEN_PRIORITY "PRIORITY"
%token <str> TOKEN_TAG "TAG"
%token <str> TOKEN_STATUS "STATUS"
%token <str> TOKEN_NAME "NAME"
%token <str> TOKEN_PATH "PATH"
%token <str> TOKEN_TIME "TIME"
%token <str> TOKEN_MTIME "MTIME"
%token <str> TOKEN_ALL "ALL"
%token <str> TOKEN_ALLOF "ALLOF"
%token <str> TOKEN_ANYOF "ANYOF"

%token <str> TOKEN_AND "AND"
%token <str> TOKEN_OR "OR"
%token <str> TOKEN_XOR "XOR"
%token <str> TOKEN_NOT "NOT"

%token TOKEN_GT "GT"
%token TOKEN_LT "LT"
%token TOKEN_GE "GE"
%token TOKEN_LE "LE"
%token TOKEN_EQ "EQ"
%token TOKEN_NE "NE"
%token TOKEN_FUZZY "FUZZY"
%token TOKEN_NFUZZY "NFUZZY"
%token TOKEN_TILDE "TILDE"
%token TOKEN_NTILDE "NTILDE"

%token TOKEN_LPAREN "LPAREN"
%token TOKEN_RPAREN "RPAREN"
%token TOKEN_COMMA "COMMA"

%token <num> TOKEN_NUMBER "NUMBER VALUE"
%token <str> TOKEN_STRING "STRING VALUE"
%token <str> TOKEN_TIMESTAMP "TIMESTAMP VALUE"

%token <str> TOKEN_UNRESOLVED "token"

%type <num> cmp_op
%type <num> string_field
%type <num> time_field
%type <num> number_field
%type <num> list_modifier
%type <num> number_value

%type <str> string_value
%type <str> time_value

%type <node> expr
%type <node> condition
%type <node> condition_number
%type <node> condition_string
%type <node> condition_time

%type <str_list> time_list
%type <str_list> string_list

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
    | TOKEN_ALL                      { free($1); $$ = create_all(); }
    | expr TOKEN_AND expr            { free($2); $$ = create_binary_op(OP_AND, $1, $3); }
    | expr TOKEN_OR expr             { free($2); $$ = create_binary_op(OP_OR, $1, $3); }
    | expr TOKEN_XOR expr            { free($2); $$ = create_binary_op(OP_XOR, $1, $3); }
    | TOKEN_NOT expr                 { free($1); $$ = create_unary_op(OP_NOT, $2); }
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
    TOKEN_NUMBER   { $$ = $1; }
    ;

number_field:
    TOKEN_PRIORITY { free($1); $$ = CMP_PRIORITY; }
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
    TOKEN_TIMESTAMP  { $$ = $1; }
    ;

time_field:
    TOKEN_TIME       { free($1); $$ = CMP_TIME; }
    | TOKEN_DEADLINE { free($1); $$ = CMP_DEADLINE; }
    | TOKEN_MTIME    { free($1); $$ = CMP_MTIME; }
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
    TOKEN_STRING     { $$ = $1; }
    | TOKEN_DEADLINE { $$ = $1; }
    | TOKEN_PRIORITY { $$ = $1; }
    | TOKEN_TAG      { $$ = $1; }
    | TOKEN_STATUS   { $$ = $1; }
    | TOKEN_NAME     { $$ = $1; }
    | TOKEN_PATH     { $$ = $1; }
    | TOKEN_TIME     { $$ = $1; }
    | TOKEN_MTIME    { $$ = $1; }
    | TOKEN_ALL      { $$ = $1; }
    | TOKEN_ALLOF    { $$ = $1; }
    | TOKEN_ANYOF    { $$ = $1; }
    | TOKEN_AND      { $$ = $1; }
    | TOKEN_OR       { $$ = $1; }
    | TOKEN_XOR      { $$ = $1; }
    | TOKEN_NOT      { $$ = $1; }
    ;

string_field:
    TOKEN_TAG      { free($1); $$ = CMP_TAG; }
    | TOKEN_STATUS { free($1); $$ = CMP_STATUS; }
    | TOKEN_PATH   { free($1); $$ = CMP_PATH; }
    | TOKEN_NAME   { free($1); $$ = CMP_NAME; }
    ;

//

cmp_op:
    TOKEN_EQ       { $$ = CMP_EQ; }
    | TOKEN_NE     { $$ = CMP_NE; }
    | TOKEN_FUZZY  { $$ = CMP_FUZZY; }
    | TOKEN_NFUZZY { $$ = CMP_NFUZZY; }
    | TOKEN_TILDE  { $$ = CMP_TILDE; }
    | TOKEN_NTILDE { $$ = CMP_NTILDE; }
    | TOKEN_GT     { $$ = CMP_GT; }
    | TOKEN_LT     { $$ = CMP_LT; }
    | TOKEN_GE     { $$ = CMP_GE; }
    | TOKEN_LE     { $$ = CMP_LE; }
    ;

list_modifier:
    TOKEN_ALLOF   { free($1); $$ = LM_ALLOF; }
    | TOKEN_ANYOF { free($1); $$ = LM_ANYOF; }
    ;

%%
