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

%token TOKEN_AND TOKEN_OR TOKEN_NOT
%token TOKEN_DEADLINE TOKEN_PRIORITY TOKEN_TAG
%token TOKEN_STATUS TOKEN_NAME TOKEN_TIME TOKEN_MTIME
%token TOKEN_ALL
%token TOKEN_ALLOF TOKEN_ANYOF
%token TOKEN_GT TOKEN_LT TOKEN_GE TOKEN_LE TOKEN_EQ
%token TOKEN_NE TOKEN_TILDE TOKEN_NTILDE
%token TOKEN_LPAREN TOKEN_RPAREN TOKEN_COMMA

%token <num> TOKEN_NUMBER
%token <str> TOKEN_STRING TOKEN_TIMESTAMP
%type <node> expr condition condition_priority condition_string condition_time
%type <str_list> string_list time_list
%type <num_list> number_list
%type <num> cmp_op string_field time_field list_modifier

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

condition_priority:
    TOKEN_PRIORITY cmp_op TOKEN_NUMBER {
        $$ = create_comparison(CMP_PRIORITY, $2, $3, NULL);
    }
    | TOKEN_PRIORITY cmp_op list_modifier TOKEN_LPAREN number_list TOKEN_RPAREN {
        $$ = expand_list(CMP_PRIORITY, $2, NULL, $5, $3);
    }
    ;

condition_time:
    time_field cmp_op TOKEN_TIMESTAMP {
        $$ = create_comparison($1, $2, 0, $3);
    }
    | time_field cmp_op list_modifier TOKEN_LPAREN time_list TOKEN_RPAREN {
        $$ = expand_list($1, $2, $5, NULL, $3);
    }
    ;

condition_string:
    string_field cmp_op TOKEN_STRING {
        $$ = create_comparison($1, $2, 0, $3);
    }
    | string_field cmp_op list_modifier TOKEN_LPAREN string_list TOKEN_RPAREN {
        $$ = expand_list($1, $2, $5, NULL, $3);
    }
    ;

string_list:
    TOKEN_STRING { $$ = create_string_list($1); }
    | string_list TOKEN_COMMA TOKEN_STRING {
        append_string($1, $3);
        $$ = $1;
    }
    ;

number_list:
    TOKEN_NUMBER { $$ = create_number_list($1); }
    | number_list TOKEN_COMMA TOKEN_NUMBER {
        append_number($1, $3);
        $$ = $1;
    }
    ;

time_list:
    TOKEN_TIMESTAMP { $$ = create_string_list($1); }
    | time_list TOKEN_COMMA TOKEN_TIMESTAMP {
        append_string($1, $3);
        $$ = $1;
    }
    ;

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

string_field:
    TOKEN_TAG      { $$ = CMP_TAG; }
    | TOKEN_STATUS { $$ = CMP_STATUS; }
    | TOKEN_NAME   { $$ = CMP_NAME; }
    ;

time_field:
    TOKEN_TIME       { $$ = CMP_TIME; }
    | TOKEN_DEADLINE { $$ = CMP_DEADLINE; }
    | TOKEN_MTIME    { $$ = CMP_MTIME; }
    ;

%%
