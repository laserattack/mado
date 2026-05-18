#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern FILE *yyin;
extern ASTNode *ast_root;
extern int yyparse(void);
extern void yyrestart(FILE *);
extern void yylex_destroy(void);

ASTNode *parse(const char *query) {
    yyin = fmemopen((void *)query, strlen(query), "r");
    if (!yyin)
        return NULL;

    ast_root = NULL;
    yyparse();
    fclose(yyin);
    yyrestart(NULL);
    yylex_destroy();

    return ast_root;
}

void ast_free(ASTNode *node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_ALL:
        break;
    case NODE_BINARY_OP:
        ast_free(node->binary.left);
        ast_free(node->binary.right);
        break;
    case NODE_UNARY_OP:
        ast_free(node->unary.expr);
        break;
    case NODE_COMPARISON:
        if (node->comparison.field == CMP_TAG ||
            node->comparison.field == CMP_STATUS ||
            node->comparison.field == CMP_NAME ||
            node->comparison.field == CMP_TIME) {
            free(node->comparison.value.str_value);
        }
        break;
    }
    free(node);
}
