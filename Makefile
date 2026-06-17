CXX = c++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
TARGET = mado
MAIN_SRC = main.cpp
LIB_DIR = libmado
INCLUDES = -I$(LIB_DIR)

all:
	$(MAKE) -C $(LIB_DIR) clean
	$(MAKE) -C $(LIB_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(MAIN_SRC) -L$(LIB_DIR) -lmado -o $(TARGET)

clean:
	$(MAKE) -C $(LIB_DIR) clean
	rm -f $(TARGET)

.PHONY: all clean
