CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE
FLEX = flex
BISON = bison

TARGET = tamd
TARGET_SRC = main.c

LEXER = lexer.l
LEX_OUT = lex.yy.c

PARSER = parser.y
PARSER_OUT = parser.tab.c
PARSER_HEADER = parser.tab.h

AST_SRC = ast.c

all: $(TARGET) $(TEST_TARGET)

$(LEX_OUT): $(LEXER)
	$(FLEX) $(LEXER)

$(PARSER_OUT) $(PARSER_HEADER): $(PARSER)
	$(BISON) -d $(PARSER)

$(TARGET): $(LEX_OUT) $(PARSER_OUT) $(TARGET_SRC) $(AST_SRC)
	$(CC) $(CFLAGS) $(LEX_OUT) $(PARSER_OUT) $(TARGET_SRC) $(AST_SRC) -o $(TARGET)

clean:
	rm -f $(LEX_OUT) $(PARSER_OUT) $(PARSER_HEADER) $(TARGET) $(TEST_TARGET)

.PHONY: all clean
