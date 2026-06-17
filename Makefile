CXX = c++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
TARGET = mado
MAIN_SRC = main.cpp
LIB_DIR = libmado
LIB = $(LIB_DIR)/libmado.a
LIB_SRC = $(wildcard $(LIB_DIR)/*.cpp $(LIB_DIR)/*.c $(LIB_DIR)/*.h $(LIB_DIR)/*.hpp $(LIB_DIR)/utils/*.h)
INCLUDES = -I$(LIB_DIR)

all: $(TARGET)

$(LIB): $(LIB_SRC)
	$(MAKE) -C $(LIB_DIR)

$(TARGET): $(MAIN_SRC) $(LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(MAIN_SRC) -L$(LIB_DIR) -lmado -o $(TARGET)

clean:
	$(MAKE) -C $(LIB_DIR) clean
	rm -f $(TARGET)

.PHONY: all clean
