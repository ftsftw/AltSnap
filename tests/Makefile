CC = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -Werror -pedantic
BUILD_DIR = ../work/linked-grid-tests
BIN = $(BUILD_DIR)/linked_grid_test$(EXEEXT)

.PHONY: test build clean
test: build
	$(BIN)

build: $(BIN)

$(BIN): linked_grid_test.c ../linked_grid.c ../linked_grid.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../linked_grid.c linked_grid_test.c $(LDFLAGS) -o $@

clean:
	rm -f $(BIN)
