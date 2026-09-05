CC = cc
CXX = c++
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
AR = ar
FLEX = flex
BISON = bison

LIB = libmado.a
QUERY_DIR = query
COMMON_DIR = common

CPP_OBJS = mado.o string_utils.o

PARSER_OBJS = $(QUERY_DIR)/ast.o \
              $(QUERY_DIR)/lex.yy.o \
              $(QUERY_DIR)/parser.tab.o

COMMON_OBJS = $(COMMON_DIR)/fuzzy_match.o \
              $(COMMON_DIR)/glob.o

OBJS = $(CPP_OBJS) $(PARSER_OBJS) $(COMMON_OBJS)

INCLUDES = -I. -I$(QUERY_DIR) -I$(COMMON_DIR)

all: $(LIB)

# parser
$(QUERY_DIR)/parser.tab.c $(QUERY_DIR)/parser.tab.h: $(QUERY_DIR)/parser.y
	cd $(QUERY_DIR) && $(BISON) -d parser.y

$(QUERY_DIR)/lex.yy.c: $(QUERY_DIR)/lexer.l $(QUERY_DIR)/parser.tab.h
	cd $(QUERY_DIR) && $(FLEX) lexer.l

$(QUERY_DIR)/%.o: $(QUERY_DIR)/%.c
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
	rm -f $(QUERY_DIR)/*.o $(QUERY_DIR)/lex.yy.c
	rm -f $(QUERY_DIR)/parser.tab.c $(QUERY_DIR)/parser.tab.h
	rm -f $(QUERY_DIR)/*.output
	rm -f $(COMMON_DIR)/*.o

.PHONY: all clean
