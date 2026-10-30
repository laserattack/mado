CXX = c++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
BIN_DIR = bin
TARGET = $(BIN_DIR)/mado
MAIN_SRC = main.cpp
LIB_DIR = libmado
INCLUDES = -I$(LIB_DIR)

all: normal

normal:
	$(MAKE) -C $(LIB_DIR) clean
	$(MAKE) -C $(LIB_DIR)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(MAIN_SRC) -L$(LIB_DIR) -lmado $(LDLIBS) -o $(TARGET)

test:
	$(MAKE) -C $(LIB_DIR) clean
	$(MAKE) -C $(LIB_DIR)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) test.cpp -L$(LIB_DIR) -lmado $(LDLIBS) -o $(BIN_DIR)/test_mado
	./$(BIN_DIR)/test_mado

release:
	$(MAKE) -C $(LIB_DIR) clean
	$(MAKE) -C $(LIB_DIR)
	mkdir -p release
	$(CXX) $(CXXFLAGS) --static $(INCLUDES) $(MAIN_SRC) -L$(LIB_DIR) -lmado $(LDLIBS) -o release/mado

etags:
	find . -type f -regex '.*\.\(c\|cpp\|h\|hpp\)$$' | etags -

clean:
	$(MAKE) -C $(LIB_DIR) clean
	rm -rf $(BIN_DIR)
	rm -f TAGS
	rm -rf release

.PHONY: all normal test release clean etags
