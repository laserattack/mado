CC = cc
CXX = c++
# _XOPEN_SOURCE=700 = _POSIX_C_SOURCE=200809L (https://www.man7.org/linux/man-pages/man7/feature_test_macros.7.html)
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
AR = ar
FLEX = flex
BISON = bison

LIB = libmado.a
OBJS = mado.o ast.o fuzzy_match.o lex.yy.o parser.tab.o

all:
	bison -d parser.y
	flex lexer.l
	$(CC) $(CFLAGS) -c lex.yy.c -o lex.yy.o
	$(CC) $(CFLAGS) -c parser.tab.c -o parser.tab.o
	$(CC) $(CFLAGS) -c ast.c -o ast.o
	$(CC) $(CFLAGS) -c fuzzy_match.c -o fuzzy_match.o
	$(CXX) $(CXXFLAGS) -c mado.cpp -o mado.o
	$(AR) rcs $(LIB) $(OBJS)

clean:
	rm -f lex.yy.c parser.tab.c parser.tab.h $(LIB) *.o *.output

.PHONY: all clean
