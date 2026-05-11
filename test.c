#include "lexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TOKEN(token, expected) \
    assert(token == expected)

#define ASSERT_NUM(value, expected) \
    assert(value == expected)

#define ASSERT_STR(value, expected) \
    assert(strcmp(value, expected) == 0)

#define RUN_TEST(test_fn) \
    do { \
        fprintf(stderr, "Running %s... ", #test_fn); \
        test_fn(); \
        fprintf(stderr, "OK\n"); \
    } while(0)

extern int yy_scan_string(const char *str);

static void test_lexer_priority_gt(void) {
    yy_scan_string("priority > 5");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();

    ASSERT_TOKEN(t1, TOKEN_PRIORITY);
    ASSERT_TOKEN(t2, TOKEN_GT);
    ASSERT_TOKEN(t3, TOKEN_NUMBER);
    ASSERT_NUM(yylval.num, 5);
}

static void test_lexer_priority_lt(void) {
    yy_scan_string("priority < 3");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();

    ASSERT_TOKEN(t1, TOKEN_PRIORITY);
    ASSERT_TOKEN(t2, TOKEN_LT);
    ASSERT_TOKEN(t3, TOKEN_NUMBER);
    ASSERT_NUM(yylval.num, 3);
}

static void test_lexer_priority_ge(void) {
    yy_scan_string("priority >= 10");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();

    ASSERT_TOKEN(t1, TOKEN_PRIORITY);
    ASSERT_TOKEN(t2, TOKEN_GE);
    ASSERT_TOKEN(t3, TOKEN_NUMBER);
    ASSERT_NUM(yylval.num, 10);
}

static void test_lexer_tag_eq_string(void) {
    yy_scan_string("tag = \"urgent\"");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();

    ASSERT_TOKEN(t1, TOKEN_TAG);
    ASSERT_TOKEN(t2, TOKEN_EQ);
    ASSERT_TOKEN(t3, TOKEN_STRING);
    ASSERT_STR(yylval.str, "urgent");
    free(yylval.str);
}

static void test_lexer_and_operator(void) {
    yy_scan_string("priority > 5 and tag = \"work\"");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();
    int t4 = yylex();
    int t5 = yylex();
    int t6 = yylex();

    ASSERT_TOKEN(t1, TOKEN_PRIORITY);
    ASSERT_TOKEN(t2, TOKEN_GT);
    ASSERT_TOKEN(t3, TOKEN_NUMBER);
    ASSERT_NUM(yylval.num, 5);
    ASSERT_TOKEN(t4, TOKEN_AND);
    ASSERT_TOKEN(t5, TOKEN_TAG);
    ASSERT_TOKEN(t6, TOKEN_EQ);
}

static void test_lexer_or_operator(void) {
    yy_scan_string("tag = \"bug\" or priority > 8");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();
    int t4 = yylex();
    int t5 = yylex();
    int t6 = yylex();

    ASSERT_TOKEN(t1, TOKEN_TAG);
    ASSERT_TOKEN(t2, TOKEN_EQ);
    ASSERT_TOKEN(t3, TOKEN_STRING);
    ASSERT_STR(yylval.str, "bug");
    free(yylval.str);
    ASSERT_TOKEN(t4, TOKEN_OR);
    ASSERT_TOKEN(t5, TOKEN_PRIORITY);
    ASSERT_TOKEN(t6, TOKEN_GT);
}

static void test_lexer_not_operator(void) {
    yy_scan_string("not priority = 1");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();
    int t4 = yylex();

    ASSERT_TOKEN(t1, TOKEN_NOT);
    ASSERT_TOKEN(t2, TOKEN_PRIORITY);
    ASSERT_TOKEN(t3, TOKEN_EQ);
    ASSERT_TOKEN(t4, TOKEN_NUMBER);
    ASSERT_NUM(yylval.num, 1);
}

static void test_lexer_parentheses(void) {
    yy_scan_string("(priority > 5) and (tag = \"urgent\")");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();
    int t4 = yylex();
    int t5 = yylex();
    int t6 = yylex();
    int t7 = yylex();
    int t8 = yylex();
    int t9 = yylex();
    int t10 = yylex();

    ASSERT_TOKEN(t1, TOKEN_LPAREN);
    ASSERT_TOKEN(t2, TOKEN_PRIORITY);
    ASSERT_TOKEN(t3, TOKEN_GT);
    ASSERT_TOKEN(t4, TOKEN_NUMBER);
    ASSERT_TOKEN(t5, TOKEN_RPAREN);
    ASSERT_TOKEN(t6, TOKEN_AND);
    ASSERT_TOKEN(t7, TOKEN_LPAREN);
    ASSERT_TOKEN(t8, TOKEN_TAG);
    ASSERT_TOKEN(t9, TOKEN_EQ);
    ASSERT_TOKEN(t10, TOKEN_STRING);
    free(yylval.str);
}

static void test_lexer_identifier_as_tag_name(void) {
    yy_scan_string("tag = important");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();

    ASSERT_TOKEN(t1, TOKEN_TAG);
    ASSERT_TOKEN(t2, TOKEN_EQ);
    ASSERT_TOKEN(t3, TOKEN_IDENT);
    ASSERT_STR(yylval.str, "important");
    free(yylval.str);
}

static void test_lexer_multiple_spaces(void) {
    yy_scan_string("priority    >     5");

    int t1 = yylex();
    int t2 = yylex();
    int t3 = yylex();

    ASSERT_TOKEN(t1, TOKEN_PRIORITY);
    ASSERT_TOKEN(t2, TOKEN_GT);
    ASSERT_TOKEN(t3, TOKEN_NUMBER);
    ASSERT_NUM(yylval.num, 5);
}

static void test_lexer_complex_expression(void) {
    yy_scan_string("(priority >= 7 or tag = \"security\") and not tag = \"done\"");

    int tokens[] = {
        TOKEN_LPAREN, TOKEN_PRIORITY, TOKEN_GE, TOKEN_NUMBER,
        TOKEN_OR, TOKEN_TAG, TOKEN_EQ, TOKEN_STRING,
        TOKEN_RPAREN, TOKEN_AND, TOKEN_NOT, TOKEN_TAG, TOKEN_EQ, TOKEN_STRING
    };

    for (int i = 0; i < sizeof(tokens) / sizeof(*tokens); i++) {
        int token = yylex();
        ASSERT_TOKEN(token, tokens[i]);
        if (token == TOKEN_NUMBER && yylval.num == 7) continue;
        if (token == TOKEN_STRING) free(yylval.str);
    }
}

int main(void) {
    RUN_TEST(test_lexer_priority_gt);
    RUN_TEST(test_lexer_priority_lt);
    RUN_TEST(test_lexer_priority_ge);
    RUN_TEST(test_lexer_tag_eq_string);
    RUN_TEST(test_lexer_and_operator);
    RUN_TEST(test_lexer_or_operator);
    RUN_TEST(test_lexer_not_operator);
    RUN_TEST(test_lexer_parentheses);
    RUN_TEST(test_lexer_identifier_as_tag_name);
    RUN_TEST(test_lexer_multiple_spaces);
    RUN_TEST(test_lexer_complex_expression);

    fprintf(stderr, "All tests passed!\n");
    return 0;
}
