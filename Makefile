CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
TARGET = mado
MAIN_SRC = main.c
LIB_DIR = libmado
LIB = $(LIB_DIR)/libmado.a
INCLUDES = -I$(LIB_DIR)

all: $(TARGET)

$(LIB):
	$(MAKE) -C $(LIB_DIR)

$(TARGET): $(MAIN_SRC) $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(MAIN_SRC) -L$(LIB_DIR) -lmado -o $(TARGET)

clean:
	$(MAKE) -C $(LIB_DIR) clean
	rm -f $(TARGET)

.PHONY: all clean
