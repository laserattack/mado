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

static ASTNode *create_comparison(ComparisonField field, ComparisonOperator cmp, int int_val, char *str_val) {
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

%}

%union {
    int num;
    char *str;
    struct ASTNode *node;
}

%token TOKEN_AND TOKEN_OR TOKEN_NOT
%token TOKEN_PRIORITY TOKEN_TAG TOKEN_STATUS TOKEN_NAME
%token TOKEN_ALL
%token TOKEN_GT TOKEN_LT TOKEN_GE TOKEN_LE TOKEN_EQ TOKEN_NE TOKEN_TILDE TOKEN_NE_TILDE
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
    | TOKEN_ALL                      { $$ = create_all(); }
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
    | TOKEN_PRIORITY TOKEN_NE TOKEN_NUMBER {
        $$ = create_comparison(CMP_PRIORITY, CMP_NE, $3, NULL);
    }
    | TOKEN_TAG TOKEN_EQ TOKEN_STRING {
        $$ = create_comparison(CMP_TAG, CMP_EQ, 0, $3);
    }
    | TOKEN_TAG TOKEN_EQ TOKEN_IDENT {
        $$ = create_comparison(CMP_TAG, CMP_EQ, 0, $3);
    }
    | TOKEN_TAG TOKEN_NE TOKEN_STRING {
        $$ = create_comparison(CMP_TAG, CMP_NE, 0, $3);
    }
    | TOKEN_TAG TOKEN_NE TOKEN_IDENT {
        $$ = create_comparison(CMP_TAG, CMP_NE, 0, $3);
    }
    | TOKEN_TAG TOKEN_TILDE TOKEN_STRING {
        $$ = create_comparison(CMP_TAG, CMP_SUBSTR, 0, $3);
    }
    | TOKEN_TAG TOKEN_TILDE TOKEN_IDENT {
        $$ = create_comparison(CMP_TAG, CMP_SUBSTR, 0, $3);
    }
    | TOKEN_TAG TOKEN_NE_TILDE TOKEN_STRING {
        $$ = create_comparison(CMP_TAG, CMP_NSUBSTR, 0, $3);
    }
    | TOKEN_TAG TOKEN_NE_TILDE TOKEN_IDENT {
        $$ = create_comparison(CMP_TAG, CMP_NSUBSTR, 0, $3);
    }
    | TOKEN_STATUS TOKEN_EQ TOKEN_STRING {
        $$ = create_comparison(CMP_STATUS, CMP_EQ, 0, $3);
    }
    | TOKEN_STATUS TOKEN_EQ TOKEN_IDENT {
        $$ = create_comparison(CMP_STATUS, CMP_EQ, 0, $3);
    }
    | TOKEN_STATUS TOKEN_NE TOKEN_STRING {
        $$ = create_comparison(CMP_STATUS, CMP_NE, 0, $3);
    }
    | TOKEN_STATUS TOKEN_NE TOKEN_IDENT {
        $$ = create_comparison(CMP_STATUS, CMP_NE, 0, $3);
    }
    | TOKEN_STATUS TOKEN_TILDE TOKEN_STRING {
        $$ = create_comparison(CMP_STATUS, CMP_SUBSTR, 0, $3);
    }
    | TOKEN_STATUS TOKEN_TILDE TOKEN_IDENT {
        $$ = create_comparison(CMP_STATUS, CMP_SUBSTR, 0, $3);
    }
    | TOKEN_STATUS TOKEN_NE_TILDE TOKEN_STRING {
        $$ = create_comparison(CMP_STATUS, CMP_NSUBSTR, 0, $3);
    }
    | TOKEN_STATUS TOKEN_NE_TILDE TOKEN_IDENT {
        $$ = create_comparison(CMP_STATUS, CMP_NSUBSTR, 0, $3);
    }
    | TOKEN_NAME TOKEN_EQ TOKEN_STRING {
        $$ = create_comparison(CMP_NAME, CMP_EQ, 0, $3);
    }
    | TOKEN_NAME TOKEN_EQ TOKEN_IDENT {
        $$ = create_comparison(CMP_NAME, CMP_EQ, 0, $3);
    }
    | TOKEN_NAME TOKEN_NE TOKEN_STRING {
        $$ = create_comparison(CMP_NAME, CMP_NE, 0, $3);
    }
    | TOKEN_NAME TOKEN_NE TOKEN_IDENT {
        $$ = create_comparison(CMP_NAME, CMP_NE, 0, $3);
    }
    | TOKEN_NAME TOKEN_TILDE TOKEN_STRING {
        $$ = create_comparison(CMP_NAME, CMP_SUBSTR, 0, $3);
    }
    | TOKEN_NAME TOKEN_TILDE TOKEN_IDENT {
        $$ = create_comparison(CMP_NAME, CMP_SUBSTR, 0, $3);
    }
    | TOKEN_NAME TOKEN_NE_TILDE TOKEN_STRING {
        $$ = create_comparison(CMP_NAME, CMP_NSUBSTR, 0, $3);
    }
    | TOKEN_NAME TOKEN_NE_TILDE TOKEN_IDENT {
        $$ = create_comparison(CMP_NAME, CMP_NSUBSTR, 0, $3);
    }
    ;

%%
