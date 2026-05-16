%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern int yylex(void);
extern void yyerror(const char *fmt, ...);

ASTNode *ast_root = NULL;

static ASTNode *create_binary_op(Operator op, ASTNode *left, ASTNode *right) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = NODE_BINARY_OP;
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    return node;
}

static ASTNode *create_unary_op(Operator op, ASTNode *expr) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = NODE_UNARY_OP;
    node->unary.op = op;
    node->unary.expr = expr;
    return node;
}

static ASTNode *create_list_comparison(ComparisonField field,
                                       ComparisonOperator cmp,
                                       NumList *num_list,
                                       StringList *str_list) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = NODE_LIST_COMPARISON;
    node->list_comparison.field = field;
    node->list_comparison.cmp = cmp;
    if (num_list) {
        node->list_comparison.num_list = num_list;
    } else {
        node->list_comparison.str_list = str_list;
    }
    return node;
}

static ASTNode *create_comparison(ComparisonField field,
                                  ComparisonOperator cmp,
                                  int int_val,
                                  char *str_val) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = NODE_COMPARISON;
    node->comparison.field = field;
    node->comparison.cmp = cmp;
    if (str_val) {
        node->comparison.value.str_value = str_val;
    } else {
        node->comparison.value.int_value = int_val;
    }
    return node;
}

static ASTNode *create_all(void) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = NODE_ALL;
    return node;
}

static StringList *create_string_list(char *first) {
    StringList *list = malloc(sizeof(StringList));
    list->items = malloc(sizeof(char*));
    list->items[0] = first;
    list->count = 1;
    return list;
}

static void append_string(StringList *list, char *item) {
    list->count++;
    list->items = realloc(list->items, list->count * sizeof(char*));
    list->items[list->count - 1] = item;
}

static NumList *create_number_list(int first) {
    NumList *list = malloc(sizeof(NumList));
    list->items = malloc(sizeof(int));
    list->items[0] = first;
    list->count = 1;
    return list;
}

static void append_number(NumList *list, int item) {
    list->count++;
    list->items = realloc(list->items, list->count * sizeof(int));
    list->items[list->count - 1] = item;
}

%}

%union {
    int num;
    char *str;
    struct StringList *str_list;
    struct NumList *num_list;
    struct ASTNode *node;
}

%token TOKEN_AND TOKEN_OR TOKEN_NOT
%token TOKEN_PRIORITY TOKEN_TAG TOKEN_STATUS TOKEN_NAME TOKEN_TIME
%token TOKEN_ALL
%token TOKEN_GT TOKEN_LT TOKEN_GE TOKEN_LE TOKEN_EQ
%token TOKEN_NE TOKEN_TILDE TOKEN_NE_TILDE
%token TOKEN_LPAREN TOKEN_RPAREN TOKEN_LBRACKET TOKEN_RBRACKET TOKEN_COMMA

%token <num> TOKEN_NUMBER
%token <str> TOKEN_IDENT TOKEN_STRING TOKEN_TIME_VALUE
%type <node> expr condition condition_priority condition_string condition_time
%type <str_list> string_list time_list
%type <num_list> number_list
%type <str> string
%type <num> cmp_op string_field

%left TOKEN_OR
%left TOKEN_AND
%right TOKEN_NOT

%start input

%%

input:
    expr { ast_root = $1; }
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
    | TOKEN_PRIORITY cmp_op TOKEN_LBRACKET number_list TOKEN_RBRACKET {
        $$ = create_list_comparison(CMP_PRIORITY, $2, $4, NULL);
    }
    ;

condition_time:
    TOKEN_TIME cmp_op TOKEN_TIME_VALUE {
        $$ = create_comparison(CMP_TIME, $2, 0, $3);
    }
    | TOKEN_TIME cmp_op TOKEN_LBRACKET time_list TOKEN_RBRACKET {
        $$ = create_list_comparison(CMP_TIME, $2, NULL, $4);
    }
    ;

condition_string:
    string_field cmp_op string {
        $$ = create_comparison($1, $2, 0, $3);
    }
    | string_field cmp_op TOKEN_LBRACKET string_list TOKEN_RBRACKET {
        $$ = create_list_comparison($1, $2, NULL, $4);
    }
    ;

string_list:
    string { $$ = create_string_list($1); }
    | string_list TOKEN_COMMA string {
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
    TOKEN_TIME_VALUE { $$ = create_string_list($1); }
    | time_list TOKEN_COMMA TOKEN_TIME_VALUE {
        append_string($1, $3);
        $$ = $1;
    }
    ;

cmp_op:
    TOKEN_EQ         { $$ = CMP_EQ; }
    | TOKEN_NE       { $$ = CMP_NE; }
    | TOKEN_TILDE    { $$ = CMP_SUBSTR; }
    | TOKEN_NE_TILDE { $$ = CMP_NSUBSTR; }
    | TOKEN_GT       { $$ = CMP_GT; }
    | TOKEN_LT       { $$ = CMP_LT; }
    | TOKEN_GE       { $$ = CMP_GE; }
    | TOKEN_LE       { $$ = CMP_LE; }
    ;

string_field:
    TOKEN_TAG      { $$ = CMP_TAG; }
    | TOKEN_STATUS { $$ = CMP_STATUS; }
    | TOKEN_NAME   { $$ = CMP_NAME; }
    ;

string:
    TOKEN_STRING
    | TOKEN_IDENT
    ;

%%
