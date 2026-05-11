CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE
FLEX = flex

TARGET = tamd
TARGET_SRC = main.c

TEST_TARGET = test_runner
TEST_SRC = test.c
TEST_LDFLAGS = -lfl

LEXER = lexer.l
LEX_OUT = lex.yy.c

all: $(TARGET) $(TEST_TARGET)

$(LEX_OUT): $(LEXER)
	$(FLEX) $(LEXER)

$(TARGET): $(LEX_OUT) $(TARGET_SRC)
	$(CC) $(CFLAGS) $(LEX_OUT) $(TARGET_SRC) -o $(TARGET)

$(TEST_TARGET): $(LEX_OUT) $(TEST_SRC)
	$(CC) $(CFLAGS) $(LEX_OUT) $(TEST_SRC) -o $(TEST_TARGET) $(TEST_LDFLAGS)

clean:
	rm -f $(LEX_OUT) $(TARGET) $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

.PHONY: all clean test
