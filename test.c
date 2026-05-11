#include "lexer.h"
#include <assert.h>

extern int yy_scan_string(const char *str);

void test_simple_expression(void) {
    const char* input = "priority > 5";
    printf("Testing: \"%s\"\n", input);

    yy_scan_string(input);

    int token1 = yylex();
    int token2 = yylex();
    int token3 = yylex();

    assert(token1 == TOKEN_PRIORITY);
    assert(token2 == TOKEN_GT);
    assert(token3 == TOKEN_NUMBER);
    assert(yylval.num == 5);
}

int main(void) {
    test_simple_expression();
    printf("Test passed!\n");
    return 0;
}
