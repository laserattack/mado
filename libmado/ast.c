#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern void set_query_string(const char *query); // in lexer, for error print
extern struct yy_buffer_state *yy_scan_string(const char *);

extern int yyparse();
extern void yyrestart(FILE *);
extern void yylex_destroy();

AST_Node *ast_root = NULL;

AST_Node *parse(const char *query) {
    set_query_string(query);
    yy_scan_string(query);
    ast_root = NULL;
    yyparse();
    yylex_destroy();
    return ast_root;
}

void ast_free(AST_Node *node) {
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
            node->comparison.field == CMP_TIME ||
            node->comparison.field == CMP_DEADLINE ||
            node->comparison.field == CMP_MTIME) {
            free(node->comparison.value.str_value);
        }
        break;
    }
    free(node);
}

AST_Node *create_binary_op(Operator op, AST_Node *left, AST_Node *right) {
    AST_Node *node = malloc(sizeof(AST_Node));
    node->type = NODE_BINARY_OP;
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    return node;
}

AST_Node *create_unary_op(Operator op, AST_Node *expr) {
    AST_Node *node = malloc(sizeof(AST_Node));
    node->type = NODE_UNARY_OP;
    node->unary.op = op;
    node->unary.expr = expr;
    return node;
}

AST_Node *create_comparison(Comparison_Field field,
                            Comparison_Operator cmp,
                            int int_val,
                            char *str_val) {
    AST_Node *node = malloc(sizeof(AST_Node));
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

AST_Node *create_all() {
    AST_Node *node = malloc(sizeof(AST_Node));
    node->type = NODE_ALL;
    return node;
}

AST_Node *expand_list(Comparison_Field field, Comparison_Operator op,
                      String_List *str_list, Num_List *num_list,
                      ListModifier lm) {
    Operator comb = (lm == LM_ALLOF) ? OP_AND : OP_OR;
    AST_Node *result = NULL;

    int count = str_list ? str_list->count : num_list->count;
    for (int i = 0; i < count; i++) {
        AST_Node *cmp = str_list
                            ? create_comparison(field, op, 0, str_list->items[i])
                            : create_comparison(field, op, num_list->items[i], NULL);

        result = result ? create_binary_op(comb, result, cmp) : cmp;
    }

    if (str_list) {
        free(str_list->items);
        free(str_list);
    } else {
        free(num_list->items);
        free(num_list);
    }

    return result;
}

String_List *create_string_list(char *first) {
    String_List *list = malloc(sizeof(String_List));
    list->items = malloc(sizeof(char *));
    list->items[0] = first;
    list->count = 1;
    return list;
}

void append_string(String_List *list, char *item) {
    list->count++;
    list->items = realloc(list->items, list->count * sizeof(char *));
    list->items[list->count - 1] = item;
}

Num_List *create_number_list(int first) {
    Num_List *list = malloc(sizeof(Num_List));
    list->items = malloc(sizeof(int));
    list->items[0] = first;
    list->count = 1;
    return list;
}

void append_number(Num_List *list, int item) {
    list->count++;
    list->items = realloc(list->items, list->count * sizeof(int));
    list->items[list->count - 1] = item;
}
