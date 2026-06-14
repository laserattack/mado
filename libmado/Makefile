CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
AR = ar
FLEX = flex
BISON = bison

LIB = libmado.a
LIB_OBJ = mado.o ast.o
LEXER = lexer.l
LEX_OUT = lex.yy.c
LEX_OBJ = lex.yy.o
PARSER = parser.y
PARSER_OUT = parser.tab.c
PARSER_HEADER = parser.tab.h
PARSER_OBJ = parser.tab.o

all: $(LIB)

$(PARSER_OUT) $(PARSER_HEADER): $(PARSER)
	$(BISON) -d $(PARSER)

$(LEX_OUT): $(LEXER) $(PARSER_HEADER)
	$(FLEX) $(LEXER)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(LEX_OBJ) $(PARSER_OBJ) $(LIB_OBJ)
	$(AR) rcs $@ $^

clean:
	rm -f $(LEX_OUT) $(PARSER_OUT) $(PARSER_HEADER) $(LIB) *.o

.PHONY: all clean
