CXX = c++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
TARGET = mado
MAIN_SRC = main.cpp
LIB_DIR = libmado
INCLUDES = -I$(LIB_DIR)

# Enable TBB support: make USE_TBB=1
ifdef USE_TBB
LDLIBS = -ltbb
endif

all:
	$(MAKE) -C $(LIB_DIR) clean
	$(MAKE) -C $(LIB_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(MAIN_SRC) -L$(LIB_DIR) -lmado $(LDLIBS) -o $(TARGET)

etags:
	find . -type f -regex '.*\.\(c\|cpp\|h\|hpp\)$$' | etags -

clean:
	$(MAKE) -C $(LIB_DIR) clean
	rm -f $(TARGET) TAGS

.PHONY: all clean etags
