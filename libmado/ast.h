#ifndef AST_H
#define AST_H

typedef enum {
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_COMPARISON,
    NODE_ALL
} Node_Type;

typedef enum { OP_AND,
               OP_OR,
               OP_NOT } Operator;

typedef enum {
    CMP_PRIORITY,
    CMP_TAG,
    CMP_STATUS,
    CMP_NAME,
    CMP_TIME,
    CMP_DEADLINE
} Comparison_Field;

typedef enum {
    CMP_GT,
    CMP_LT,
    CMP_EQ,
    CMP_NE,
    CMP_GE,
    CMP_LE,
    CMP_TILDE,
    CMP_NTILDE
} Comparison_Operator;

typedef struct AST_Node {
    Node_Type type;
    union {
        struct {
            Operator op;
            struct AST_Node *left;
            struct AST_Node *right;
        } binary;
        struct {
            Operator op;
            struct AST_Node *expr;
        } unary;
        struct {
            Comparison_Field field;
            Comparison_Operator cmp;
            union {
                int int_value;
                char *str_value;
            } value;
        } comparison;
    };
} AST_Node;

typedef struct String_List {
    char **items;
    int count;
} String_List;

typedef struct Num_List {
    int *items;
    int count;
} Num_List;

typedef enum { LM_ALLOF,
               LM_ANYOF } ListModifier;

AST_Node *parse(const char *query);
void ast_free(AST_Node *node);
AST_Node *create_binary_op(Operator op, AST_Node *left, AST_Node *right);
AST_Node *create_unary_op(Operator op, AST_Node *expr);
AST_Node *create_comparison(Comparison_Field field, Comparison_Operator cmp, int int_val, char *str_val);
AST_Node *create_all();
AST_Node *expand_list(Comparison_Field field, Comparison_Operator op,
                      String_List *str_list, Num_List *num_list,
                      ListModifier lm);

String_List *create_string_list(char *first);
Num_List *create_number_list(int first);
void append_string(String_List *list, char *item);
void append_number(Num_List *list, int item);

#endif
