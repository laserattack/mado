CC = cc
CXX = c++
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
AR = ar
FLEX = flex
BISON = bison

LIB = libmado.a
PARSER_DIR = parser
COMMON_DIR = common

CPP_OBJS = mado.o

PARSER_OBJS = $(PARSER_DIR)/ast.o \
              $(PARSER_DIR)/lex.yy.o \
              $(PARSER_DIR)/parser.tab.o

COMMON_OBJS = $(COMMON_DIR)/fuzzy_match.o \
              $(COMMON_DIR)/glob.o

OBJS = $(CPP_OBJS) $(PARSER_OBJS) $(COMMON_OBJS)

INCLUDES = -I. -I$(PARSER_DIR) -I$(COMMON_DIR)

all: $(LIB)

# parser
$(PARSER_DIR)/parser.tab.c $(PARSER_DIR)/parser.tab.h: $(PARSER_DIR)/parser.y
	cd $(PARSER_DIR) && $(BISON) -d parser.y

$(PARSER_DIR)/lex.yy.c: $(PARSER_DIR)/lexer.l $(PARSER_DIR)/parser.tab.h
	cd $(PARSER_DIR) && $(FLEX) lexer.l

$(PARSER_DIR)/%.o: $(PARSER_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# common
$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

# cpp
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# all
$(LIB): $(OBJS)
	$(AR) rcs $(LIB) $(OBJS)

clean:
	rm -f $(LIB) *.o
	rm -f $(PARSER_DIR)/*.o $(PARSER_DIR)/lex.yy.c
	rm -f $(PARSER_DIR)/parser.tab.c $(PARSER_DIR)/parser.tab.h
	rm -f $(PARSER_DIR)/*.output
	rm -f $(COMMON_DIR)/*.o

.PHONY: all clean
