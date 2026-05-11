#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern FILE *yyin;
extern ASTNode *ast_root;
extern int yyparse(void);
extern void yyrestart(FILE *);
extern void yylex_destroy(void);

ASTNode* parse(const char *query) {
    yyin = fmemopen((void*)query, strlen(query), "r");
    if (!yyin) return NULL;

    ast_root = NULL;
    yyparse();
    fclose(yyin);
    yyrestart(NULL);
    yylex_destroy();

    return ast_root;
}

void ast_free(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_BINARY_OP:
            ast_free(node->binary.left);
            ast_free(node->binary.right);
            break;
        case NODE_UNARY_OP:
            ast_free(node->unary.expr);
            break;
        case NODE_COMPARISON:
            if (node->comparison.field == CMP_TAG ||
                node->comparison.field == CMP_STATUS) {
                free(node->comparison.value.str_value);
            }
            break;
    }
    free(node);
}

void ast_print(ASTNode *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->type) {
        case NODE_BINARY_OP:
            switch (node->binary.op) {
                case OP_AND: printf("AND\n"); break;
                case OP_OR:  printf("OR\n"); break;
                default:     printf("BINARY\n"); break;
            }
            ast_print(node->binary.left, indent + 1);
            ast_print(node->binary.right, indent + 1);
            break;

        case NODE_UNARY_OP:
            printf("NOT\n");
            ast_print(node->unary.expr, indent + 1);
            break;

        case NODE_COMPARISON:
            switch (node->comparison.field) {
                case CMP_PRIORITY: printf("priority "); break;
                case CMP_TAG:      printf("tag "); break;
                case CMP_STATUS:   printf("status "); break;
            }
            switch (node->comparison.cmp) {
                case CMP_GT: printf("> "); break;
                case CMP_LT: printf("< "); break;
                case CMP_EQ: printf("= "); break;
                case CMP_GE: printf(">= "); break;
                case CMP_LE: printf("<= "); break;
            }
            if (node->comparison.field == CMP_PRIORITY) {
                printf("%d\n", node->comparison.value.int_value);
            } else {
                printf("\"%s\"\n", node->comparison.value.str_value);
            }
            break;
    }
}
