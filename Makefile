CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
FLEX = flex
BISON = bison

TARGET = mado
TARGET_SRC = main.c

MADO_SRC = mado.c

LEXER = lexer.l
LEX_OUT = lex.yy.c

PARSER = parser.y
PARSER_OUT = parser.tab.c
PARSER_HEADER = parser.tab.h

AST_SRC = ast.c

all: $(TARGET)

$(LEX_OUT): $(LEXER)
	$(FLEX) $(LEXER)

$(PARSER_OUT) $(PARSER_HEADER): $(PARSER)
	$(BISON) -d $(PARSER)

$(TARGET): $(LEX_OUT) $(PARSER_OUT) $(TARGET_SRC) $(AST_SRC) $(MADO_SRC)
	$(CC) $(CFLAGS) $(LEX_OUT) $(PARSER_OUT) $(TARGET_SRC) $(AST_SRC) $(MADO_SRC) -o $(TARGET)

llm:
	gitingest

etags:
	find . -name "*.[ch]" | etags -

clean:
	rm -f $(LEX_OUT) $(PARSER_OUT) $(PARSER_HEADER) $(TARGET) digest.txt TAGS

.PHONY: all clean llm etags
