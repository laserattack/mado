#ifndef AST_H
#define AST_H

typedef enum {
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_COMPARISON,
    NODE_LIST_COMPARISON,
    NODE_ALL
} NodeType;

typedef enum { OP_AND, OP_OR, OP_NOT } Operator;

typedef enum {
    CMP_PRIORITY,
    CMP_TAG,
    CMP_STATUS,
    CMP_NAME,
    CMP_TIME
} ComparisonField;

typedef enum {
    CMP_GT,
    CMP_LT,
    CMP_EQ,
    CMP_NE,
    CMP_SUBSTR,
    CMP_NSUBSTR,
    CMP_GE,
    CMP_LE
} ComparisonOperator;

typedef struct StringList {
    char **items;
    int count;
} StringList;

typedef struct NumList {
    int *items;
    int count;
} NumList;

typedef struct ASTNode {
    NodeType type;
    union {
        struct {
            Operator op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;
        struct {
            Operator op;
            struct ASTNode *expr;
        } unary;
        struct {
            ComparisonField field;
            ComparisonOperator cmp;
            union {
                int int_value;
                char *str_value;
            } value;
        } comparison;
        struct {
            ComparisonField field;
            ComparisonOperator cmp;
            union {
                StringList *str_list;
                NumList *num_list;
            };
        } list_comparison;
    };
} ASTNode;

extern ASTNode *parse(const char *query);
extern void ast_free(ASTNode *node);
extern void ast_print(ASTNode *node, int indent);

#endif
