#include <stdio.h>

#include "ast.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s \"query\"\n", argv[0]);
        fprintf(stderr, "Example: %s \"priority > 5\"\n", argv[0]);
        return 1;
    }

    const char *query = argv[1];
    printf("Query: %s\n", query);

    ASTNode *ast = parse(query);
    if (!ast) {
        fprintf(stderr, "Failed to parse query\n");
        return 1;
    }

    printf("AST:\n");
    ast_print(ast, 0);

    ast_free(ast);
    return 0;
}
