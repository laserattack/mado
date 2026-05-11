%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern int yylex(void);
extern void yyerror(const char *s);

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

static ASTNode *create_comparison(ComparisonField field, ComparisonOperator cmp, int int_val, char *str_val) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = NODE_COMPARISON;
    node->comparison.field = field;
    node->comparison.cmp = cmp;
    if (str_val) {
        node->comparison.value.str_value = strdup(str_val);
    } else {
        node->comparison.value.int_value = int_val;
    }
    return node;
}

%}

%union {
    int num;
    char *str;
    struct ASTNode *node;
}

%token TOKEN_AND TOKEN_OR TOKEN_NOT
%token TOKEN_PRIORITY TOKEN_TAG
%token TOKEN_GT TOKEN_LT TOKEN_GE TOKEN_LE TOKEN_EQ
%token TOKEN_LPAREN TOKEN_RPAREN
%token <num> TOKEN_NUMBER
%token <str> TOKEN_IDENT TOKEN_STRING

%type <node> expr condition

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
    | expr TOKEN_AND expr            { $$ = create_binary_op(OP_AND, $1, $3); }
    | expr TOKEN_OR expr             { $$ = create_binary_op(OP_OR, $1, $3); }
    | TOKEN_NOT expr                 { $$ = create_unary_op(OP_NOT, $2); }
    | TOKEN_LPAREN expr TOKEN_RPAREN { $$ = $2; }
    ;

condition:
    TOKEN_PRIORITY TOKEN_GT TOKEN_NUMBER {
        $$ = create_comparison(CMP_PRIORITY, CMP_GT, $3, NULL);
    }
    | TOKEN_PRIORITY TOKEN_LT TOKEN_NUMBER {
        $$ = create_comparison(CMP_PRIORITY, CMP_LT, $3, NULL);
    }
    | TOKEN_PRIORITY TOKEN_GE TOKEN_NUMBER {
        $$ = create_comparison(CMP_PRIORITY, CMP_GE, $3, NULL);
    }
    | TOKEN_PRIORITY TOKEN_LE TOKEN_NUMBER {
        $$ = create_comparison(CMP_PRIORITY, CMP_LE, $3, NULL);
    }
    | TOKEN_PRIORITY TOKEN_EQ TOKEN_NUMBER {
        $$ = create_comparison(CMP_PRIORITY, CMP_EQ, $3, NULL);
    }
    | TOKEN_TAG TOKEN_EQ TOKEN_STRING {
        $$ = create_comparison(CMP_TAG, CMP_EQ, 0, $3);
    }
    | TOKEN_TAG TOKEN_EQ TOKEN_IDENT {
        $$ = create_comparison(CMP_TAG, CMP_EQ, 0, $3);
    }
    ;

%%
